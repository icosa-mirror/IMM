// CPU-only functional test. Fixture contains groups/spawn areas, no GPU resources.
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <thread>
#include "libImmPlayer/src/document.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

static void require(bool ok, const char * message)
{
    if (!ok) { std::fprintf(stderr, "[IMM_VIEWPOINT_POLICY_TEST] %s\n", message); std::exit(1); }
}

int wmain(int argc, wchar_t ** argv)
{
    require(argc == 2, "expected spawn-only IMM fixture path");
    piLog log;
    require(log.Init(L"artifacts/viewpoint-policy-runtime.log", PILOG_TXT), "log init");
    Document doc;
    require(doc.Init(L"viewpoint-policy", 0), "document init");
    auto tick = [&](double seconds, const Document::Command * command = nullptr)
    {
        const piTick now = piTick::FromSeconds(seconds);
        doc.UpdateStateCPU(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, nullptr, &log, now, command);
        doc.UpdateStateGPU(nullptr, nullptr, nullptr, nullptr, &log, Drawing::ColorSpace::Linear);
    };
    Document::Command load{};
    load.mType = Document::Command::Type::Load;
    load.mFileType = Document::ImportType::IMM_disk;
    require(load.mStrArg.Init(1024) && load.mStrArg.CopyW(argv[1]), "fixture path");
    tick(0, &load);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (doc.GetLoadingState() != Document::LoadingState::Loaded &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        tick(0);
    }
    require(doc.GetLoadingState() == Document::LoadingState::Loaded, "fixture load");
    require(doc.GetSpawnAreaCount() == 3, "legacy enumeration includes all viewpoints");
    int nested = -1, visible = -1, selfHidden = -1;
    for (int id = 0; id < doc.GetSpawnAreaCount(); ++id)
    {
        Document::SpawnAreaInfo info;
        doc.GetSpawnAreaInfo(info, id);
        if (std::wcscmp(info.mName, L"nested") == 0) nested = id;
        if (std::wcscmp(info.mName, L"visible") == 0) visible = id;
        if (std::wcscmp(info.mName, L"self-hidden") == 0) selfHidden = id;
    }
    require(nested >= 0 && visible >= 0 && selfHidden >= 0, "fixture viewpoint names");
    require(!doc.IsSpawnAreaAvailable(-1) && !doc.IsSpawnAreaAvailable(3), "invalid viewpoint IDs");
    require(!doc.IsSpawnAreaAvailable(nested), "hidden ancestor excludes nested viewpoint");
    require(!doc.IsSpawnAreaAvailable(selfHidden), "self visibility excludes viewpoint");
    require(doc.IsSpawnAreaAvailable(visible), "visible viewpoint available");
    doc.GetSequence()->GetRoot()->SetVisibilityOverride(true, false);
    require(doc.GetAvailableSpawnAreaIds(0, nullptr) == 0, "hidden root yields empty availability");
    doc.GetSequence()->GetRoot()->SetVisibilityOverride(false, false);
    require(doc.GetAvailableSpawnAreaIds(0, nullptr) == 1, "count-only query");
    require(doc.GetAvailableSpawnAreaIds(-1, nullptr) == -1, "invalid capacity");
    require(doc.GetAvailableSpawnAreaIds(1, nullptr) == -1, "invalid buffer");
    SpawnAreaPoseState pose{};
    doc.SetSpawnArea(nested);
    require(doc.GetSpawnArea() == nested, "legacy selection remains unrestricted");
    require(!doc.TrySetActiveSpawnAreaId(selfHidden, 0, pose) && doc.GetSpawnArea() == nested,
        "rejected checked selection preserves active viewpoint");
    SpawnAreaSnapshot state{};
    int ids[3] = {-1, -1, -1};
    require(doc.GetSpawnAreaSnapshot(state, 3, ids) == 1 && ids[0] == visible,
        "snapshot list and flags agree");
    require(state.activeAvailable == 0 && state.preferredId == visible, "active/preferred remain separate");
    require(!doc.TrySetActiveSpawnAreaId(visible, state.evaluationRevision + 1, pose), "stale revision rejected");
    require(doc.TrySetActiveSpawnAreaId(visible, state.evaluationRevision, pose), "checked selection succeeds");

    Document::Command pause{};
    pause.mType = Document::Command::Type::Pause;
    tick(0, &pause);
    require(doc.GetPlaybackState() == Document::PlaybackState::Paused, "pause fixture");
    Document::Command seek{};
    seek.mType = Document::Command::Type::RequestSetTime;
    seek.mEvaluationRequest = doc.BeginPlaybackEvaluationRequest();
    seek.mSeekStartTicks = piTick::CastInt(piTick::FromSeconds(2.5));
    seek.mSeekStopTicks = 1;
    require(!doc.TrySetActiveSpawnAreaId(visible, 0, pose), "pending request rejects checked selection");
    doc.UpdateStateCPU(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
        Drawing::PaintRenderingTechnique::Static, nullptr, &log, piTick::FromSeconds(10), &pause, &seek);
    require(doc.GetSpawnAreaSnapshot(state, 0, nullptr) == 1 &&
        state.commandStatus == static_cast<int32_t>(PlaybackRequestStatus::Pending),
        "temporarily masked command remains pending");
    tick(10, &seek);
    require(doc.GetSpawnAreaSnapshot(state, 1, ids) == 3 && state.availableCount == 3,
        "short buffer returns full required count after paused evaluation");
    require(state.commandStatus == static_cast<int32_t>(PlaybackRequestStatus::Applied) &&
        state.completedCommand == seek.mEvaluationRequest, "tracked seek completed after evaluation");
    require(doc.GetPlaybackState() == Document::PlaybackState::Paused, "tracked seek preserves pause");
    require(state.preferredId == nested && state.needsUpdate == 1 && state.activeId == visible,
        "MakeDefault changes preference/signal, not active selection");
    require(doc.TrySetActiveSpawnAreaId(nested, state.evaluationRevision, pose), "newly visible selection");
    require(pose.posx > 4.9f, "selection pose includes evaluated parent transform");
    require(doc.GetSpawnAreaNeedsUpdate(), "checked selection does not acknowledge authored request");

    Document::Command chapter{};
    chapter.mType = Document::Command::Type::RequestSetChapter;
    chapter.mEvaluationRequest = doc.BeginPlaybackEvaluationRequest();
    chapter.mIntArg = 999;
    tick(10, &chapter);
    require(doc.GetSpawnAreaSnapshot(state, 0, nullptr) == 3 &&
        state.commandStatus == static_cast<int32_t>(PlaybackRequestStatus::Rejected), "invalid chapter completion");
    chapter.mEvaluationRequest = doc.BeginPlaybackEvaluationRequest();
    chapter.mIntArg = 0;
    tick(10, &chapter);
    require(doc.GetSpawnAreaSnapshot(state, 3, ids) == 1 &&
        state.commandStatus == static_cast<int32_t>(PlaybackRequestStatus::Applied) &&
        state.activeId == nested && state.activeAvailable == 0,
        "chapter seek reevaluates visibility without automatically changing active selection");
    doc.BeginPlaybackEvaluationRequest();
    tick(10, &pause);
    require(doc.GetSpawnAreaSnapshot(state, 0, nullptr) == 1 &&
        state.commandStatus == static_cast<int32_t>(PlaybackRequestStatus::Superseded), "legacy command supersedes request");
    // Legacy SetTime must evaluate and hold a new pose even when already paused.
    const piTick seekNow = piTick::FromSeconds(10);
    const piTick seekPosition = piTick::FromSeconds(2.5);
    doc.SetTime(seekNow, seekPosition, piTick(1));
    piTick positionBefore, stoppedBefore, positionAfter, stoppedAfter;
    doc.GetTime(seekNow, &positionBefore, &stoppedBefore);
    tick(12, &pause);
    doc.GetTime(piTick::FromSeconds(12), &positionAfter, &stoppedAfter);
    require(positionBefore == seekPosition && positionAfter == positionBefore,
        "legacy paused seek holds its requested time after a repeated Pause");
    require(doc.GetAvailableSpawnAreaIds(0, nullptr) == 3,
        "legacy paused seek evaluates animated viewpoint visibility");
    doc.SetTime(piTick::FromSeconds(12), seekPosition, piTick(0));
    doc.GetTime(piTick::FromSeconds(12), &positionBefore, &stoppedBefore);
    tick(14, &pause);
    doc.GetTime(piTick::FromSeconds(14), &positionAfter, &stoppedAfter);
    require(positionAfter == positionBefore && stoppedAfter > stoppedBefore,
        "zero-stop paused seek remains paused");
    Document::Command resume{};
    resume.mType = Document::Command::Type::Resume;
    tick(16, &resume);
    doc.GetTime(piTick::FromSeconds(16), &positionBefore, &stoppedBefore);
    tick(18);
    doc.GetTime(piTick::FromSeconds(18), &positionAfter, &stoppedAfter);
    require(positionAfter > positionBefore, "resume restarts the root clock after a paused seek");
    doc.UnloadSync(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
        Drawing::PaintRenderingTechnique::Static, nullptr, nullptr, &log, piTick::FromSeconds(10));
    require(!doc.IsSpawnAreaAvailable(visible), "unloaded viewpoint unavailable");
    require(doc.GetSpawnAreaSnapshot(state, 0, nullptr) == -1, "unloaded snapshot rejected");
    doc.End();
    load.mStrArg.End();
    log.End();
    std::puts("[IMM_VIEWPOINT_POLICY_TEST] passed");
    return 0;
}
