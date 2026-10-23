using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;
using N = ImmPlayer.ImmLiveEditNative;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif

namespace ImmPlayer
{
    /// <summary>Interactive, in-memory native live editing of real IMM documents.</summary>
    public sealed class ImmLiveEditPanel : MonoBehaviour
    {
        private const string Prefix = "[IMM_LIVE_PANEL]";
        private ImmDocument document;
        private ImmPlayerManager manager;
        private ImmDocument.LayerInfo[] layers = Array.Empty<ImmDocument.LayerInfo>();
        private int selected = -1;
        private N.Properties properties;
        private N.Pivot pivot;
        private N.Status commit;
        private N.Revisions revisions;
        private ulong pending;
        private bool attached, loading, visible = true, pivotSupported, keyVisible = true;
        private string file = "", status = "Load a real IMM file to begin.", layerName = "";
        private string seek = "0", keyTime = "0", frame = "0", repeat = "1", parent = "0", childIndex = "0";
        private string markerSize = "1";
        private string newLayerName = "Live layer";
        private string positionX = "0", positionY = "0", positionZ = "0";
        private string filter = "", evaluated = "";
        private float nextDiagnostics;
        private GUIStyle wrapped;
        private int keyChannel;
        private Vector2 scroll, layerScroll;
        private Rect window;
        private Coroutine load;
        private Camera view;
        private readonly string[] channels = { "Visibility", "Opacity", "Transform", "Loop", "Offset ticks" };
        private readonly int[] channelIds = { 0, 1, 9, 7, 8 };

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
            if (attached && selected >= 0 && Time.unscaledTime >= nextDiagnostics)
            {
                nextDiagnostics = Time.unscaledTime + 0.2f;
                Run(ReadDiagnostics);
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
                            Log($"Revision {completed}: Presented");
                        }
                        else
                        {
                            string reason = $"Revision {completed}: Rejected — {N.Result(commit.result)}; command {commit.command}, target {commit.target}";
                            Run(Refresh);
                            Log(reason);
                        }
                    }
                }
                catch (Exception error) { pending = 0; Log(error.Message); }
            }
#if ENABLE_INPUT_SYSTEM
            if (Keyboard.current != null && Keyboard.current.f9Key.wasPressedThisFrame) visible = !visible;
            if (view != null && Mouse.current != null && Keyboard.current != null && Mouse.current.rightButton.isPressed &&
                (!visible || !window.Contains(new Vector2(Mouse.current.position.x.ReadValue(), Screen.height - Mouse.current.position.y.ReadValue()))))
            {
                var delta = Mouse.current.delta.ReadValue();
                view.transform.rotation = Quaternion.Euler(-delta.y * 0.12f, delta.x * 0.12f, 0) * view.transform.rotation;
                var keyboard = Keyboard.current;
                var movement = new Vector3((keyboard.dKey.isPressed ? 1 : 0) - (keyboard.aKey.isPressed ? 1 : 0),
                    (keyboard.eKey.isPressed ? 1 : 0) - (keyboard.qKey.isPressed ? 1 : 0),
                    (keyboard.wKey.isPressed ? 1 : 0) - (keyboard.sKey.isPressed ? 1 : 0));
                view.transform.Translate(movement * Time.unscaledDeltaTime * (keyboard.leftShiftKey.isPressed ? 8 : 2));
            }
#endif
        }

        private void OnGUI()
        {
            if (!visible) return;
            window.height = Mathf.Max(250, Screen.height - 24);
            window.width = Mathf.Min(440, Screen.width - 24);
            Color tint = GUI.color;
            GUI.color = new Color(0.12f, 0.14f, 0.18f, 1);
            GUI.DrawTexture(window, Texture2D.whiteTexture);
            GUI.color = tint;
            window = GUILayout.Window(GetEntityId().GetHashCode(), window, DrawPanel, "IMM Live Edit Lab — native edits");
        }

        private void DrawPanel(int id)
        {
            if (wrapped == null) wrapped = new GUIStyle(GUI.skin.label) { wordWrap = true };
            scroll = GUILayout.BeginScrollView(scroll);
            GUILayout.Label("Memory only. Reload restores the file. F9 hides panel.", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.BeginHorizontal();
            file = GUILayout.TextField(file, GUILayout.Width(Mathf.Max(80, window.width - 125)));
#if UNITY_EDITOR
            if (GUILayout.Button("Browse", GUILayout.Width(65)))
            {
                string picked = UnityEditor.EditorUtility.OpenFilePanel("Open IMM", "", "imm");
                if (!string.IsNullOrEmpty(picked)) file = picked;
            }
#endif
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUI.enabled = !loading && pending == 0;
            if (GUILayout.Button("Load / Reload")) BeginLoad();
            GUI.enabled = document != null || loading;
            if (GUILayout.Button("Unload")) Unload();
            GUI.enabled = document != null && !loading;
            if (GUILayout.Button("Frame scene")) FrameScene();
            GUILayout.EndHorizontal();
            GUILayout.Label(status, wrapped, GUILayout.Width(window.width - 50), GUILayout.MinHeight(38));
            if (pending != 0) GUILayout.Label($"Revision {pending}: {StateName(commit.state)}");
            GUILayout.Label($"Requested {revisions.requested} / Prepared {revisions.prepared} / Presented {revisions.presented}");
            GUI.enabled = document != null && !loading;
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Play")) document.Resume();
            if (GUILayout.Button("Pause")) document.Pause();
            if (GUILayout.Button("Restart")) document.Restart();
            seek = GUILayout.TextField(seek, GUILayout.Width(55));
            if (GUILayout.Button("Seek s")) Run(() => { document.SetTime(checked((long)(Number(seek) * 12600)), 0); });
            GUILayout.EndHorizontal();
            GUI.enabled = attached && pending == 0 && !loading;
            if (GUILayout.Button($"Refresh layers ({layers.Length})")) Run(Refresh);
            GUILayout.Label("Filter layer name/path or ID");
            filter = GUILayout.TextField(filter, GUILayout.Width(window.width - 50));
            layerScroll = GUILayout.BeginScrollView(layerScroll, GUILayout.Height(145));
            for (int index = 0; index < layers.Length; ++index)
            {
                var layer = layers[index];
                if (!string.IsNullOrEmpty(filter) && !layer.FullName.Contains(filter, StringComparison.OrdinalIgnoreCase) &&
                    layer.Id.ToString(CultureInfo.InvariantCulture) != filter) continue;
                if (GUILayout.Toggle(selected == layer.Id, $"{layer.Id}  {layer.Type}  {layer.FullName}", "Button", GUILayout.Width(window.width - 56)) && selected != layer.Id)
                { selected = layer.Id; Run(ReadSelected); }
            }
            GUILayout.EndScrollView();
            GUI.enabled &= selected >= 0;
            GUILayout.Label($"Selected layer {selected}");
            GUILayout.Label(evaluated, wrapped, GUILayout.Width(window.width - 50));
            GUILayout.Label("Canonical properties — animation keys may override these.", wrapped, GUILayout.Width(window.width - 50));
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
            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Apply position")) ChangeProperties("Transform", 4, ReadPositionFields);
            if (GUILayout.Button("+0.1m X")) ChangeProperties("Move X", 4, () => properties.tx += 0.1f);
            if (GUILayout.Button("Mirror X")) ChangeProperties("Reflection", 4, () => properties.flip = properties.flip == 1 ? 0 : 1);
            GUILayout.EndHorizontal();
            GUILayout.Label($"Pivot ({pivot.tx:F2}, {pivot.ty:F2}, {pivot.tz:F2})");
            GUI.enabled &= pivotSupported;
            if (GUILayout.Button("Pivot +0.1m X")) Submit("Pivot", () => { pivot.tx += 0.1f; return N.ImmAuthoring_LayerSetPivot(Doc, selected, ref pivot); });
            GUI.enabled = attached && pending == 0 && !loading && selected >= 0;
            GUILayout.BeginHorizontal();
            repeat = GUILayout.TextField(repeat, GUILayout.Width(70));
            if (GUILayout.Button("Set paint repeat (0 = loop)")) Submit("Paint repeat", () => N.ImmAuthoring_PaintSetMaxRepeatCount(Doc, selected, Unsigned(repeat)));
            GUILayout.EndHorizontal();
            GUILayout.Space(8);
            GUILayout.Label("Keys — uses current inspector values at the entered time", wrapped, GUILayout.Width(window.width - 50));
            keyChannel = GUILayout.SelectionGrid(keyChannel, channels, 3);
            if (keyChannel == 0) keyVisible = GUILayout.Toggle(keyVisible, "Key visible");
            GUILayout.BeginHorizontal();
            keyTime = GUILayout.TextField(keyTime, GUILayout.Width(65));
            if (GUILayout.Button("Set key")) Submit("Set key", SetKey);
            if (GUILayout.Button("Remove key")) Submit("Remove key", () => N.ImmAuthoring_KeyRemove(Doc, selected, channelIds[keyChannel], Number(keyTime)));
            GUILayout.EndHorizontal();
            GUILayout.Label("Loop key uses repeat=0 as true; Offset uses repeat field as exact ticks.", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.Space(8);
            GUILayout.Label("Drawing replacement — changes the selected frame's drawing", wrapped, GUILayout.Width(window.width - 50));
            GUILayout.BeginHorizontal();
            GUILayout.Label("Frame", GUILayout.Width(40)); frame = GUILayout.TextField(frame, GUILayout.Width(50));
            GUILayout.Label("Marker m", GUILayout.Width(65)); markerSize = GUILayout.TextField(markerSize, GUILayout.Width(55));
            GUILayout.EndHorizontal();
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
            if (!attached || pending != 0) return;
            try
            {
                N.Check(queue());
                N.Check(N.ImmAuthoring_Commit(Doc, out pending));
                ReadRevisions();
                commit = N.Status.Empty; commit.state = 1;
                Log($"{label}: queued revision {pending}");
            }
            catch (Exception error)
            {
                try { N.ImmAuthoring_DiscardPending(Doc); } catch (Exception) { }
                Log($"{label}: {error.Message}");
                Run(ReadSelected);
            }
        }

        private int SetKey()
        {
            if (keyChannel == 2) ReadPositionFields();
            var key = new N.Key { size = 80, version = 1, property = channelIds[keyChannel], seconds = Number(keyTime),
                boolean = keyChannel == 3 ? (Unsigned(repeat) == 0 ? 1 : 0) : (keyVisible ? 1 : 0),
                value = properties.opacity, tx = properties.tx, ty = properties.ty, tz = properties.tz,
                qx = properties.qx, qy = properties.qy, qz = properties.qz, qw = properties.qw,
                scale = properties.scale, flip = properties.flip, ticks = keyChannel == 4 ? Unsigned(repeat) : 0 };
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
            bool found = false;
            foreach (var layer in layers) if (layer.Id == selected) found = true;
            if (!found) selected = layers.Length > 0 ? layers[0].Id : -1;
            ReadRevisions();
            ReadSelected();
        }

        private void ReadRevisions()
        {
            revisions = new N.Revisions { size = 32, version = 1 };
            N.Check(N.ImmAuthoring_GetRevisions(Doc, ref revisions));
        }

        private void ReadSelected()
        {
            if (selected < 0) return;
            properties = N.Properties.Empty;
            N.Check(N.ImmAuthoring_LayerGetProperties(Doc, selected, ref properties));
            positionX = properties.tx.ToString("G6", CultureInfo.InvariantCulture);
            positionY = properties.ty.ToString("G6", CultureInfo.InvariantCulture);
            positionZ = properties.tz.ToString("G6", CultureInfo.InvariantCulture);
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
                    if (result == 0) { attached = true; loading = false; Run(Refresh); FrameScene(); Log($"Attached live session: {Path.GetFileName(path)}"); yield break; }
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
            if (load != null) { StopCoroutine(load); load = null; }
            loading = false; attached = false; pending = 0;
            if (document != null)
            {
                retiring = document.DocumentId;
                if (manager != null) manager.UnloadDocument(document);
                document = null;
            }
            layers = Array.Empty<ImmDocument.LayerInfo>(); selected = -1; revisions = default;
            Log("Unloaded. Reload restores the original file; no edits were saved.");
        }

        private void FrameScene()
        {
            if (view == null || document == null) return;
            Bounds bounds = document.GetBoundingBox();
            float radius = Mathf.Max(0.1f, bounds.extents.magnitude);
            view.transform.position = bounds.center - view.transform.forward * radius / Mathf.Sin(view.fieldOfView * Mathf.Deg2Rad * 0.5f) * 1.15f;
            view.nearClipPlane = Mathf.Max(0.01f, radius / 1000);
            view.farClipPlane = Mathf.Max(1000, radius * 20);
        }

        private void OnDestroy()
        {
            if (document != null && manager != null) Unload();
        }
    }
}
