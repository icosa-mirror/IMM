using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;

namespace ImmPlayer
{
    /// <summary>Interactive, in-memory native live editing of real IMM documents.</summary>
    public sealed partial class ImmLiveEditPanel : MonoBehaviour
    {
        private const string Prefix = "[IMM_LIVE_PANEL]";
        private readonly ImmLiveEditInput labInput = new ImmLiveEditInput();
        private ImmDocument document;
        private ImmPlayerManager manager;
        private ImmDocument.LayerInfo[] layers = Array.Empty<ImmDocument.LayerInfo>();
        private int selected = -1;
        private N.Properties properties;
        private N.Pivot pivot;
        private N.Status commit;
        private N.Revisions revisions;
        private ulong pending;
        private bool awaitingCommit;
        private string awaitingCommitLabel;
        private float commitDeadline;
        private bool attached, loading, visible = true, pivotSupported, keyVisible = true;
        private string file = "", status = "Load a real IMM file to begin.", layerName = "";
        private string seek = "0", keyTime = "0", frame = "0", repeat = "1", parent = "0", childIndex = "0";
        private string markerSize = "1";
        private string newLayerName = "Live layer";
        private string positionX = "0", positionY = "0", positionZ = "0";
        private string filter = "", evaluated = "";
        private float nextDiagnostics;
        private GUIStyle wrapped;
        private GUIStyle treeButton;
        private sealed class LayerNode
        {
            internal ImmDocument.LayerInfo Info;
            internal LayerNode Parent;
            internal readonly List<LayerNode> Children = new List<LayerNode>();
        }
        private readonly List<LayerNode> treeRoots = new List<LayerNode>();
        private readonly Dictionary<int, LayerNode> treeNodes = new Dictionary<int, LayerNode>();
        private readonly HashSet<int> collapsedNodes = new HashSet<int>();
        private readonly HashSet<int> filteredNodes = new HashSet<int>();
        private int keyChannel, keyInterpolation;
        private int selectedViewpoint = -1;
        private int[] viewpoints = Array.Empty<int>();
        private Vector2 scroll, layerScroll;
        private Rect window;
        private Coroutine load;
        private Camera view;
        private readonly string[] channels = { "Visibility", "Opacity", "Transform", "Loop", "Offset ticks", "Draw-in" };
        private readonly int[] channelIds = { 0, 1, 9, 7, 8, 5 };

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void Bootstrap()
        {
            bool enabled = Array.IndexOf(Environment.GetCommandLineArgs(), "-immLiveEdit") >= 0;
#if UNITY_EDITOR
            enabled |= UnityEditor.SessionState.GetBool("IMM.LiveEditPanel", false);
#endif
            if (enabled && FindAnyObjectByType<ImmLiveEditPanel>() == null)
                new GameObject("IMM Live Edit Lab").AddComponent<ImmLiveEditPanel>();
        }

        private void Awake()
        {
            N.ValidateLayouts();
            window = new Rect(12, 12, 440, Mathf.Max(250, Screen.height - 24));
            view = Camera.main;
#if UNITY_EDITOR
            file = UnityEditor.SessionState.GetString("IMM.LiveEditFile", "");
#endif
            // This panel owns loading. Disable the sample's transient-override demo.
            foreach (var example in FindObjectsByType<ImmFeatureExamples>())
            {
                example.StopAllCoroutines();
                example.UnloadDocument();
                example.enabled = false;
            }
            Log("Native live editing enabled; edits affect memory only. Reload restores the source file.");
        }

        private void Update()
        {
            if (awaitingCommit) Run(TryPublishQueuedCommit);
            UpdateTimelinePlayback();
            UpdateTimelineClock();
            if (attached && selected >= 0 && Time.unscaledTime >= nextDiagnostics)
            {
                nextDiagnostics = Time.unscaledTime + 0.2f;
                Run(ReadDiagnostics);
                ReadViewpoints();
            }
            if (pending != 0 && document != null)
            {
                try
                {
                    commit = N.Status.Empty;
                    N.Check(N.ImmAuthoring_GetCommitStatus(document.DocumentId, pending, ref commit));
                    ReadRevisions();
                    if (commit.state == 4 || commit.state == 5)
                    {
                        ulong completed = pending;
                        pending = 0;
                        if (commit.state == 4)
                        {
                            Refresh();
                            FinishTimelineHistory(true);
                            Log($"Revision {completed}: Presented");
                        }
                        else
                        {
                            string reason = $"Revision {completed}: Rejected — {N.Result(commit.result)}; command {commit.command}, target {commit.target}";
                            Run(Refresh);
                            FinishTimelineHistory(false);
                            Log(reason);
                        }
                    }
                }
                catch (Exception error) { pending = 0; AbandonHistoryRequest(); Log(error.Message); }
            }
            if (view != null && labInput.Read(out bool looking, out Vector2 pointer, out Vector2 motion, out Vector3 movement, out bool faster) && looking &&
                (!visible || !OverPanel(new Vector2(pointer.x, Screen.height - pointer.y))))
            {
                view.transform.rotation = Quaternion.Euler(-motion.y * 0.12f, motion.x * 0.12f, 0) * view.transform.rotation;
                view.transform.Translate(movement * Time.unscaledDeltaTime * (faster ? 8 : 2));
            }

        }

        private void OnGUI()
        {
            if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.F9)
            {
                visible = !visible; if (!visible) ReleaseTimelineCapture(); Event.current.Use();
            }
            if (!visible) return;
            if (Event.current.type == EventType.KeyDown && (Event.current.control || Event.current.command) && GUIUtility.keyboardControl == 0 && scrubControl == 0)
            {
                if (Event.current.keyCode == KeyCode.Z)
                { Run(() => ReplayTimelineHistory(!Event.current.shift)); Event.current.Use(); }
                else if (Event.current.keyCode == KeyCode.Y)
                { Run(() => ReplayTimelineHistory(false)); Event.current.Use(); }
                else if (Event.current.keyCode == KeyCode.C)
                { Run(() => CopyTimelineKey(false)); Event.current.Use(); }
                else if (Event.current.keyCode == KeyCode.X)
                { Run(() => CopyTimelineKey(true)); Event.current.Use(); }
                else if (Event.current.keyCode == KeyCode.V)
                { Run(PasteTimelineKey); Event.current.Use(); }
            }
            DrawWorkspace();
        }

        private void DrawPanel(int id)
        {
            scroll = GUILayout.BeginScrollView(scroll);
            GUI.enabled = attached && pending == 0 && !awaitingCommit && !loading && PreviewCanEditSelected;
            GUI.enabled &= selected >= 0;
            GUILayout.Label($"Selected layer {selected}");
            GUILayout.Label(evaluated, wrapped, GUILayout.Width(window.width - 50));
            GUILayout.Label("Values — Apply edits the base layer; Set key edits animation.", wrapped, GUILayout.Width(window.width - 50));
            layerName = GUILayout.TextField(layerName, GUILayout.Width(window.width - 50));
            if (GUILayout.Button("Rename")) RenameSelected();
            GUILayout.BeginHorizontal();
            if (GUILayout.Button(properties.visible != 0 ? "Canonical visible: on" : "Canonical visible: off"))
                ChangeProperties("Visibility", 1, () => properties.visible = properties.visible == 0 ? 1 : 0);
            GUILayout.Label($"Opacity {properties.opacity:F2}", GUILayout.Width(94));
            float opacity = GUILayout.HorizontalSlider(properties.opacity, 0, 1);
            if (!Mathf.Approximately(opacity, properties.opacity)) properties.opacity = opacity;
            if (GUILayout.Button("Apply", GUILayout.Width(50))) ChangeProperties("Opacity", 2, () => { });
            GUILayout.EndHorizontal();
            GUILayout.Label("Local position (metres)");
            GUILayout.BeginHorizontal();
            positionX = GUILayout.TextField(positionX); positionY = GUILayout.TextField(positionY); positionZ = GUILayout.TextField(positionZ);
            GUILayout.EndHorizontal();
            DrawTransformFields();
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Apply transform")) ChangeProperties("Transform", 4, ReadTransformFields);
            if (GUILayout.Button("+0.1m X")) ChangeProperties("Move X", 4, () => properties.tx += 0.1f);
            if (GUILayout.Button("Mirror X")) ChangeProperties("Reflection", 4, () => properties.flip = properties.flip == 1 ? 0 : 1);
            GUILayout.EndHorizontal();
            GUILayout.Label($"Pivot ({pivot.tx:F2}, {pivot.ty:F2}, {pivot.tz:F2})");
            GUI.enabled &= pivotSupported;
            if (GUILayout.Button("Pivot +0.1m X")) Submit("Pivot", () => { pivot.tx += 0.1f; return N.ImmAuthoring_LayerSetPivot(Doc, selected, ref pivot); });
            GUI.enabled = attached && pending == 0 && !awaitingCommit && !loading && selected >= 0 && PreviewCanEditSelected;
            GUILayout.BeginHorizontal();
            repeat = GUILayout.TextField(repeat, GUILayout.Width(70));
            if (GUILayout.Button("Set paint repeat (0 = loop)")) Submit("Paint repeat", () =>
            { RecordTimelineBefore(selected, -2); return N.ImmAuthoring_PaintSetMaxRepeatCount(Doc, selected, Unsigned(repeat)); });
            GUILayout.EndHorizontal();
            GUILayout.Space(8);
            bool inspectorEditable = GUI.enabled;
            GUI.enabled &= treeNodes.TryGetValue(selected, out LayerNode keyNode) && InTimelineScope(keyNode);
            GUILayout.Label("Keys — uses current inspector values at the entered time", wrapped, GUILayout.Width(window.width - 50));
            keyChannel = GUILayout.SelectionGrid(keyChannel, channels, 3);
            bool keyInputsEnabled = GUI.enabled;
            bool discreteKey = keyChannel == 0 || keyChannel == 3 || keyChannel == 4;
            if (discreteKey) keyInterpolation = 0;
            GUI.enabled &= !discreteKey;
            keyInterpolation = GUILayout.SelectionGrid(keyInterpolation, new[] { "Step", "Linear", "Smooth", "Ease in", "Ease out", "Spline", "Auto" }, 3);
            GUI.enabled = keyInputsEnabled;
            if (keyChannel == 0) keyVisible = GUILayout.Toggle(keyVisible, "Key visible");
            if (keyChannel == 1)
            {
                GUILayout.Label($"Key opacity {properties.opacity:F3}");
                properties.opacity = GUILayout.HorizontalSlider(properties.opacity, 0, 1);
            }
            if (keyChannel == 3) keyLoop = GUILayout.Toggle(keyLoop, "Loop indefinitely");
            if (keyChannel == 4)
            {
                GUILayout.Label("Offset (ticks, 12600 per second)");
                keyOffset = GUILayout.TextField(keyOffset);
            }
            if (keyChannel == 5)
            {
                GUILayout.Label("Draw-in progress (finite, non-negative)");
                keyDrawIn = GUILayout.TextField(keyDrawIn);
                bool drawInPreview = SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Direct3D11 ||
                    SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Direct3D12;
                GUILayout.Label(drawInPreview ? "Uses per-stroke timing." : "This preview cannot display the draw-in effect.", wrapped);
            }
            GUI.enabled &= CanEditTimelineChannel(selected, channelIds[keyChannel]);
            GUILayout.BeginHorizontal();
            keyTime = GUILayout.TextField(keyTime, GUILayout.Width(65));
            if (GUILayout.Button("Set key")) Submit("Set key", SetKey);
            if (GUILayout.Button("Remove key")) Submit("Remove key", () =>
            { RecordTimelineBefore(selected, channelIds[keyChannel]); return N.ImmAuthoring_KeyRemove(Doc, selected, channelIds[keyChannel], Number(keyTime)); });
            GUILayout.EndHorizontal();
            bool canEditKey = GUI.enabled;
            GUI.enabled &= selectedTimelineKey.HasValue;
            if (GUILayout.Button("Move selected key to entered time")) Run(() => MoveTimelineKey(Number(keyTime)));
            GUI.enabled = canEditKey;
            GUI.enabled = inspectorEditable;
            GUILayout.Space(8);
            GUILayout.Label("Drawing replacement — changes the selected frame's drawing", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.BeginHorizontal();
            GUILayout.Label("Frame", GUILayout.Width(40)); frame = GUILayout.TextField(frame, GUILayout.Width(50));
            GUILayout.Label("Marker m", GUILayout.Width(65)); markerSize = GUILayout.TextField(markerSize, GUILayout.Width(55));
            GUILayout.EndHorizontal();
            DrawTimelineFrameTools();
            GUI.enabled &= !TimelinePreviewActive;
            if (GUILayout.Button("Replace frame's drawing with marker at local origin")) Submit("Drawing geometry", ReplaceDrawing);
            GUILayout.Space(8);
            GUILayout.Label("Hierarchy — parent is a layer ID, index is its child position", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.BeginHorizontal();
            GUILayout.Label("Parent", GUILayout.Width(45)); parent = GUILayout.TextField(parent, GUILayout.Width(55));
            GUILayout.Label("Index", GUILayout.Width(40)); childIndex = GUILayout.TextField(childIndex, GUILayout.Width(45));
            if (GUILayout.Button("Reparent")) Submit("Reparent", () => N.ImmAuthoring_LayerReparent(Doc, selected, Integer(parent), Integer(childIndex)));
            GUILayout.EndHorizontal();
            GUILayout.Label("New child name");
            newLayerName = GUILayout.TextField(newLayerName, GUILayout.Width(window.width - 50));
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("New child group")) CreateLayer(0);
            if (GUILayout.Button("New child paint")) CreateLayer(1);
            if (GUILayout.Button("New child spawn")) CreateLayer(2);
            GUILayout.EndHorizontal();
            if (GUILayout.Button("Delete selected subtree (reload to restore)"))
                Submit("Delete subtree", () => N.ImmAuthoring_LayerDestroy(Doc, selected));
            GUI.enabled = true;
            GUILayout.Label("Right mouse + WASD to fly; Q/E down/up; Shift faster.", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.EndScrollView();
        }

        private int Doc => document.DocumentId;
        private static double Number(string text) => double.Parse(text, NumberStyles.Float, CultureInfo.InvariantCulture);
        private static int Integer(string text) => int.Parse(text, CultureInfo.InvariantCulture);
        private static uint Unsigned(string text) => uint.Parse(text, CultureInfo.InvariantCulture);
        private void ReadPositionFields()
        {
            properties.tx = (float)Number(positionX); properties.ty = (float)Number(positionY); properties.tz = (float)Number(positionZ);
        }
        private static string StateName(int state) => state == 1 ? "Queued" : state == 2 ? "Preparing" : state == 3 ? "Prepared" : state == 4 ? "Presented" : state == 5 ? "Rejected" : "Unknown";
        private void Log(string message) { status = message; Debug.Log($"{Prefix} {message}"); }
        private void Run(Action action) { try { action(); } catch (Exception error) { Log(error.Message); } }

        private void ChangeProperties(string label, uint mask, Action update)
        {
            Submit(label, () => { update(); properties.mask = mask; return N.ImmAuthoring_LayerSetProperties(Doc, selected, ref properties); });
        }

        private void Submit(string label, Func<int> queue)
        {
            if (!attached || pending != 0 || awaitingCommit || !PreviewCanEditSelected) return;
            historyRecording = null;
            try
            {
                N.Check(queue());
                awaitingCommit = true; awaitingCommitLabel = label;
                commitDeadline = Time.unscaledTime + 5;
                TryPublishQueuedCommit();
                if (awaitingCommit) Log($"{label}: waiting for the pending playback command");
            }
            catch (Exception error)
            {
                awaitingCommit = false; AbandonHistoryRequest();
                try { N.ImmAuthoring_DiscardPending(Doc); } catch (Exception) { }
                Log($"{label}: {error.Message}");
                Run(ReadSelected);
            }
        }

        private void TryPublishQueuedCommit()
        {
            if (!awaitingCommit || document == null) return;
            int result = N.ImmAuthoring_Commit(Doc, out ulong revision);
            if (result == -7 && Time.unscaledTime < commitDeadline) return;
            awaitingCommit = false;
            if (result != 0)
            {
                N.ImmAuthoring_DiscardPending(Doc);
                AbandonHistoryRequest();
                Log($"{awaitingCommitLabel}: {N.Result(result)}. No edit was committed.");
                Run(ReadSelected);
                return;
            }
            pending = revision; ReadRevisions();
            commit = N.Status.Empty; commit.state = 1;
            Log($"{awaitingCommitLabel}: queued revision {pending}");
        }

        private int SetKey()
        {
            RecordTimelineBefore(selected, channelIds[keyChannel]);
            if (keyChannel == 2) ReadTransformFields();
            var key = new N.Key { size = 80, version = 1, property = channelIds[keyChannel], interpolation = keyInterpolation, seconds = Number(keyTime),
                boolean = keyChannel == 3 ? (keyLoop ? 1 : 0) : (keyVisible ? 1 : 0),
                value = properties.opacity, tx = properties.tx, ty = properties.ty, tz = properties.tz,
                qx = properties.qx, qy = properties.qy, qz = properties.qz, qw = properties.qw,
                scale = properties.scale, flip = properties.flip, ticks = keyChannel == 4 ? Unsigned(keyOffset) : 0 };
            if (keyChannel == 5) key.doubleValue = Number(keyDrawIn);
            if (keyChannel == 0 || keyChannel == 3 || keyChannel == 4) key.interpolation = 0;
            return N.ImmAuthoring_KeySet(Doc, selected, ref key);
        }

        private int ReplaceDrawing()
        {
            float size = (float)Number(markerSize);
            if (!(size > 0) || float.IsInfinity(size)) throw new ArgumentException("Marker size must be finite and positive.");
            N.Check(N.ImmAuthoring_FrameGetDrawingHandle(Doc, selected, Integer(frame), out ulong drawing));
            return N.ReplaceWithMarker(Doc, selected, drawing, size);
        }

        private void CreateLayer(int type)
        {
            Submit("Create layer", () => { var bytes = Encoding.UTF8.GetBytes(string.IsNullOrEmpty(newLayerName) ? "Live layer" : newLayerName);
                return N.ImmAuthoring_LayerCreate(Doc, selected, type, bytes, (uint)bytes.Length, out _); });
        }

        private void RenameSelected() => Submit("Rename", () =>
        { var bytes = Encoding.UTF8.GetBytes(layerName); return N.ImmAuthoring_LayerSetName(Doc, selected, bytes, (uint)bytes.Length); });

        private void Refresh()
        {
            layers = document.GetLayersManaged();
            BuildTree();
            ReadTimeline();
            EnsureTimelineScope();
            ReadViewpoints();
            bool found = false;
            foreach (var layer in layers) if (layer.Id == selected) found = true;
            if (!found) { selected = layers.Length > 0 ? layers[0].Id : -1; if (selected >= 0) expandedKeyLayers.Add(selected); }
            ReadRevisions();
            if (selectedTimelineKey.HasValue && selectedKeyLayer == selected) SelectTimelineKey(selected, selectedTimelineKey.Value);
            else ReadSelected();
        }

        private void BuildTree()
        {
            treeRoots.Clear(); treeNodes.Clear();
            foreach (var layer in layers) treeNodes[layer.Id] = new LayerNode { Info = layer };
            foreach (var layer in layers)
            {
                LayerNode node = treeNodes[layer.Id];
                if (layer.ParentId != layer.Id && treeNodes.TryGetValue(layer.ParentId, out LayerNode parentNode))
                { node.Parent = parentNode; parentNode.Children.Add(node); }
                else treeRoots.Add(node);
            }
            FilterTree();
        }

        private void FilterTree()
        {
            filteredNodes.Clear();
            if (string.IsNullOrEmpty(filter)) return;
            foreach (var pair in treeNodes)
            {
                LayerNode node = pair.Value;
                if (!node.Info.FullName.Contains(filter, StringComparison.OrdinalIgnoreCase) &&
                    node.Info.Id.ToString(CultureInfo.InvariantCulture) != filter) continue;
                for (LayerNode ancestor = node; ancestor != null; ancestor = ancestor.Parent)
                    filteredNodes.Add(ancestor.Info.Id);
            }
        }

        private void DrawLayerNode(LayerNode node, int depth)
        {
            bool filtering = !string.IsNullOrEmpty(filter);
            if (filtering && !filteredNodes.Contains(node.Info.Id)) return;
            bool expanded = filtering || !collapsedNodes.Contains(node.Info.Id);
            GUILayout.BeginHorizontal();
            GUILayout.Space(depth * 18);
            if (node.Children.Count > 0)
            {
                if (GUILayout.Button(expanded ? "▼" : "▶", GUILayout.Width(22)))
                { if (expanded) collapsedNodes.Add(node.Info.Id); else collapsedNodes.Remove(node.Info.Id); }
            }
            else GUILayout.Space(26);
            string label = string.IsNullOrEmpty(node.Info.Name) ? "(unnamed)" : node.Info.Name;
            var content = new GUIContent(label, node.Info.FullName);
            if (GUILayout.Toggle(selected == node.Info.Id, content, treeButton,
                GUILayout.Width(Mathf.Max(110, window.width - 170 - depth * 18))) && selected != node.Info.Id)
            { selected = node.Info.Id; Run(ReadSelected); }
            GUILayout.Label($"{node.Info.Type}  {node.Info.Id}", GUILayout.Width(88));
            GUILayout.EndHorizontal();
            if (expanded) foreach (var child in node.Children) DrawLayerNode(child, depth + 1);
        }

        private void ReadRevisions()
        {
            revisions = new N.Revisions { size = 32, version = 1 };
            N.Check(N.ImmAuthoring_GetRevisions(Doc, ref revisions));
        }

        private void ReadViewpoints()
        {
            if (TimelinePreviewActive) return;
            viewpoints = document.GetAvailableSpawnAreaIds();
            if (Array.IndexOf(viewpoints, selectedViewpoint) < 0)
            {
                int active = document.GetActiveSpawnAreaId();
                selectedViewpoint = Array.IndexOf(viewpoints, active) >= 0 ? active : viewpoints.Length > 0 ? viewpoints[0] : -1;
            }
        }

        private void MoveToViewpoint()
        {
            if (view == null || document == null || !document.TryGetSpawnAreaWorldPose(selectedViewpoint, transform, out Pose pose))
            { Log("No evaluated viewpoint is available at this time."); return; }
            view.transform.SetPositionAndRotation(pose.position, pose.rotation);
            view.nearClipPlane = 0.01f;
            view.farClipPlane = 10000;
            Log($"Moved to viewpoint {Array.IndexOf(viewpoints, selectedViewpoint) + 1}");
        }

        private void ReadSelected()
        {
            if (selected < 0) return;
            properties = N.Properties.Empty;
            N.Check(N.ImmAuthoring_LayerGetProperties(Doc, selected, ref properties));
            SyncTransformFields();
            layerName = N.ReadName(Doc, selected);
            pivot = N.Pivot.Empty;
            pivotSupported = N.ImmAuthoring_LayerGetPivot(Doc, selected, ref pivot) == 0;
            ReadDiagnostics();
            foreach (var layer in layers) if (layer.Id == selected) parent = layer.ParentId.ToString(CultureInfo.InvariantCulture);
        }

        private void ReadDiagnostics()
        {
            var value = document.GetLayerDiagnostics(selected);
            if (value.HasValue)
            {
                var d = value.Value;
                evaluated = $"Evaluated visible: {d.IsVisible} (world {d.IsWorldVisible}), opacity {d.Opacity:F2}\nKeys: visibility {d.HasVisibilityKeys}, opacity {d.HasOpacityKeys}, transform {d.HasTransformKeys}";
            }
        }

        private void BeginLoad()
        {
            Run(() => { string path = Path.GetFullPath(file); if (!File.Exists(path)) throw new FileNotFoundException("IMM file not found", path);
                Unload(); file = path; load = StartCoroutine(LoadFile(path)); });
        }

        private IEnumerator LoadFile(string path)
        {
            loading = true;
            // Wait for native retirement of the prior document before reusing IDs.
            float deadline = Time.realtimeSinceStartup + 30;
            while (retiring >= 0 && ImmNativePlugin.IsDocumentActive(retiring))
            { if (Time.realtimeSinceStartup > deadline) { loading = false; Log("Previous unload is still pending; retry Load."); yield break; } yield return null; }
            retiring = -1;
            manager = ImmPlayerManager.Instance;
            while ((!manager.IsInitialized || ImmNativePlugin.IsReadyForDocumentLoad() == 0) && Time.realtimeSinceStartup < deadline)
                yield return null;
            if (!manager.IsInitialized || ImmNativePlugin.IsReadyForDocumentLoad() == 0)
            {
                loading = false;
                Log($"Renderer is not ready for loading ({SystemInfo.graphicsDeviceType}). Check the supported backend in the Editor log.");
                yield break;
            }
            Run(() => document = manager.LoadDocument(path));
            if (document == null) { loading = false; Log("Native load could not start."); yield break; }
            document.Show(); document.Resume();
            while (document != null)
            {
                if (document.GetStateInfo().Loading == ImmDocument.LoadingState.Failed) break;
                if (document.IsSequenceReady())
                {
                    int result = N.ImmAuthoring_Attach(Doc);
                    if (result == 0)
                    {
                        attached = true; loading = false; Run(Refresh);
                        if (selectedViewpoint >= 0) MoveToViewpoint(); else FrameScene();
                        Log($"Attached live session: {Path.GetFileName(path)}"); yield break;
                    }
                    if (result != -7 && result != -4) { Log($"Attach: {N.Result(result)}"); break; }
                }
                if (Time.realtimeSinceStartup > deadline)
                    status = "Loading/importing is still in progress. Unload cancels it.";
                yield return null;
            }
            loading = false;
            Log("Load/attach did not complete. Unload and inspect the Editor log for IMM_LIVE_PANEL.");
        }

        private int retiring = -1;
        private void Unload()
        {
            RestorePreviewCamera();
            ReleaseTimelineCapture();
            ClearTimelineHistory();
            timelineScope = -1; timelineClockValid = false; timelineClockMissing = false; timelineClockError = "";
            timelineViews.Clear();
            timelinePlayTime = 0; queuedTimelineSeekTicks = null; pauseBeforeTimelineSeek = false; timelinePauseRequested = false;
            timelineSeekRequest = 0; revealTimelinePlayhead = false;
            if (load != null) { StopCoroutine(load); load = null; }
            loading = false; attached = false; pending = 0; awaitingCommit = false;
            if (document != null)
            {
                retiring = document.DocumentId;
                if (manager != null) manager.UnloadDocument(document);
                document = null;
            }
            layers = Array.Empty<ImmDocument.LayerInfo>(); selected = -1; revisions = default;
            treeRoots.Clear(); treeNodes.Clear(); collapsedNodes.Clear(); filteredNodes.Clear(); layerScroll = Vector2.zero;
            timelineData.Clear(); expandedKeyLayers.Clear(); selectedTimelineKey = null;
            viewpoints = Array.Empty<int>(); selectedViewpoint = -1;
            Log("Unloaded. Reload restores the original file; no edits were saved.");
        }

        private void FrameScene()
        {
            if (view == null || document == null) return;
            if (TimelinePreviewActive) { FramePreviewContents(); return; }
            FrameBounds(document.GetBoundingBox());
        }

        private void FrameSelected()
        {
            foreach (var layer in layers)
                if (layer.Id == selected && layer.HasBounds)
                {
                    Run(() =>
                    {
                        var pose = N.Pivot.Empty;
                        N.Check(N.ImmAuthoring_LayerGetWorldTransform(Doc, selected, ref pose));
                        Vector3 scale = Vector3.one * pose.scale;
                        if (pose.flip >= 1 && pose.flip <= 3) scale[pose.flip - 1] = -scale[pose.flip - 1];
                        // The lab leaves native document placement at identity.
                        // Its bounds/transforms already use the display-world
                        // coordinates passed to the camera; do not reflect Z again.
                        Matrix4x4 matrix = Matrix4x4.TRS(new Vector3(pose.tx, pose.ty, pose.tz),
                            new Quaternion(pose.qx, pose.qy, pose.qz, pose.qw), scale);
                        Bounds local = layer.Bounds;
                        Bounds world = new Bounds(matrix.MultiplyPoint3x4(local.center), Vector3.zero);
                        for (int corner = 0; corner < 8; corner++)
                            world.Encapsulate(matrix.MultiplyPoint3x4(local.center + Vector3.Scale(local.extents,
                                new Vector3((corner & 1) == 0 ? -1 : 1, (corner & 2) == 0 ? -1 : 1, (corner & 4) == 0 ? -1 : 1))));
                        FrameBounds(world);
                    });
                    return;
                }
            Log("Select a layer with visible bounds first.");
        }

        private void FrameBounds(Bounds bounds)
        {
            if (view == null) return;
            float radius = Mathf.Max(0.1f, bounds.extents.magnitude);
            Rect area = visible ? new Rect(window.xMax + 8, toolbarWindow.yMax + 8,
                Mathf.Max(1, Screen.width - window.xMax - 20), Mathf.Max(1, timelineWindow.y - toolbarWindow.yMax - 16)) :
                new Rect(0, 0, Screen.width, Screen.height);
            float fraction = Mathf.Max(0.05f, Mathf.Min(area.height / Screen.height, area.width / Screen.width * view.aspect));
            float halfFov = view.fieldOfView * Mathf.Deg2Rad * 0.5f;
            float distance = radius / (Mathf.Sin(halfFov) * fraction) * 1.15f;
            float x = (area.center.x / Screen.width * 2 - 1) * view.aspect;
            float y = 1 - area.center.y / Screen.height * 2;
            view.transform.position = bounds.center - view.transform.forward * distance -
                (view.transform.right * x + view.transform.up * y) * distance * Mathf.Tan(halfFov);
            view.nearClipPlane = Mathf.Max(0.01f, radius / 1000);
            view.farClipPlane = Mathf.Max(1000, distance + radius * 4);
        }

        private void OnDisable()
        {
            ReleaseTimelineCapture();
            if ((TimelinePreviewActive || previewCameraSaved) && attached && document != null)
                Run(() =>
                {
                    int result = N.ImmAuthoring_TimelinePreviewControl(Doc, timelineScope, 4, 0);
                    // Busy retains the in-flight edit and requests native exit
                    // immediately after its publication/terminal rejection.
                    if (result != -7) N.Check(result);
                });
            RestorePreviewCamera();
        }

        private void OnDestroy()
        {
            ReleaseTimelineCapture();
            if (document != null && manager != null) Unload();
        }
    }
}
