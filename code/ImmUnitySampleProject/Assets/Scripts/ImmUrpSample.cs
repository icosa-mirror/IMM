using System.Collections;
using ImmPlayer;
using UnityEngine;

/// <summary>Exposes the shared sample player to the URP validation harness.</summary>
[RequireComponent(typeof(ImmPlayerExample))]
public sealed class ImmUrpSample : MonoBehaviour
{
    private ImmPlayerExample player;
    private ImmPlayerManager manager;
    public ImmDocument Document => Player.Document;
    public string DocumentPath => Player.LoadedDocumentPath;
    public Camera DocumentCamera => Player.DocumentCamera;
    public bool IsReady => Player.InitialViewpointReady;
    private ImmPlayerExample Player => player != null ? player : player = GetComponent<ImmPlayerExample>();
    public void SetViewingOrigin(Transform origin) => Player.SetViewingOrigin(origin);

    private void Awake() => manager = ImmPlayerManager.Instance;

    private IEnumerator Start()
    {
        while (!IsReady) yield return null;
        Debug.Log($"[IMM_URP_SAMPLE] Document ready at its authored viewpoint position={DocumentCamera.transform.position}.");
    }

    private void OnDestroy()
    {
        if (manager == null) return;
        manager.Shutdown();
        Destroy(manager.gameObject);
    }
}
