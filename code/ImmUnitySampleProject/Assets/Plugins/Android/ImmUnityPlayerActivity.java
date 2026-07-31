package com.immersivefoundation.imm;

import android.content.pm.ApplicationInfo;
import android.util.Log;

import com.unity3d.player.UnityPlayerActivity;

public final class ImmUnityPlayerActivity extends UnityPlayerActivity
{
    private static final String ValidationArgument = "-force-vulkan-layers";
    private static final String LogMarker = "[IMM_UNITY_VK_VALIDATION_ARGS_20260731]";

    @Override
    protected String updateUnityCommandLineArguments(String commandLine)
    {
        String arguments = commandLine == null ? "" : commandLine.trim();
        boolean debuggable = (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
        if (debuggable && !arguments.contains(ValidationArgument))
        {
            arguments = arguments.isEmpty()
                ? ValidationArgument
                : arguments + " " + ValidationArgument;
        }

        Log.i("ImmUnityPlayerActivity", String.format(
            "%s debuggable=%s arguments=%s",
            LogMarker,
            debuggable,
            arguments));
        return arguments;
    }
}
