package org.linuxfoundation.imm.godot.ftltest;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class GodotVulkanFtlInstrumentation extends Instrumentation {
    private static final String PACKAGE_NAME = "org.linuxfoundation.imm.godot.sample";
    private static final String CAPTURE_NAME = "vulkan_visual_smoke.png";

    private int waitSeconds = 60;

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        if (arguments != null) {
            waitSeconds = parseInt(arguments.getString("waitSeconds"), waitSeconds);
        }
        start();
    }

    @Override
    public void onStart() {
        super.onStart();
        Bundle results = new Bundle();
        try {
            runSmoke(results);
            results.putString("result", "passed");
            finish(Activity.RESULT_OK, results);
        } catch (Throwable error) {
            results.putString("result", "failed");
            results.putString("error", error.toString());
            finish(1, results);
        }
    }

    private void runSmoke(Bundle results) throws Exception {
        Context targetContext = getTargetContext();
        File artifactDir = targetContext.getExternalFilesDir(null);
        require(artifactDir != null, "Target external files directory was not available");
        File logcatPath = new File(artifactDir, "logcat_after.txt");
        File capturePath = new File(artifactDir, CAPTURE_NAME);

        runShell("logcat -c");

        Intent intent = targetContext.getPackageManager().getLaunchIntentForPackage(PACKAGE_NAME);
        require(intent != null, "Launch intent not found for " + PACKAGE_NAME);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        targetContext.startActivity(intent);

        String logcat = waitForSmoke(capturePath);
        writeText(logcatPath, logcat);
        results.putString("logcat_path", logcatPath.getAbsolutePath());
        results.putString("capture_path", capturePath.getAbsolutePath());

        List<String> failures = new ArrayList<>();
        requireMarker(logcat, "IMM Godot Vulkan visual smoke passed", failures);
        requireMarker(logcat, "Loaded in CPU", failures);
        requireMarker(logcat, "Loaded in GPU", failures);
        requireMarker(logcat, "Vulkan renderer submitted picture draw commands", failures);
        requireMarker(logcat, "Vulkan renderer submitted static paint draw commands", failures);
        requireMarker(logcat, "last_vulkan_frame_started", failures);
        forbidMarker(logcat, "Fatal signal", failures);
        forbidMarker(logcat, "ImmViewerNode setup failed", failures);
        forbidMarker(logcat, "ImmViewerCompositorEffect setup failed", failures);
        forbidMarker(logcat, "visual smoke failures", failures);
        if (!capturePath.isFile() || capturePath.length() <= 0L) {
            failures.add("Godot visual smoke capture missing or empty: " + capturePath);
        }
        if (!failures.isEmpty()) {
            throw new AssertionError(String.join("; ", failures));
        }
    }

    private String waitForSmoke(File capturePath) throws Exception {
        long deadline = SystemClock.elapsedRealtime() + waitSeconds * 1000L;
        String logcat = "";
        while (SystemClock.elapsedRealtime() < deadline) {
            SystemClock.sleep(1000L);
            logcat = runShell("logcat -d");
            if (logcat.contains("IMM Godot Vulkan visual smoke passed") && capturePath.isFile() && capturePath.length() > 0L) {
                return logcat;
            }
            if (logcat.contains("Fatal signal") || logcat.contains("visual smoke failures")) {
                return logcat;
            }
        }
        return runShell("logcat -d");
    }

    private String runShell(String command) throws Exception {
        ParcelFileDescriptor descriptor = getUiAutomation().executeShellCommand(command);
        try (FileInputStream input = new FileInputStream(descriptor.getFileDescriptor());
             ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
            return output.toString(StandardCharsets.UTF_8.name());
        } finally {
            descriptor.close();
        }
    }

    private static void writeText(File path, String content) throws Exception {
        try (FileOutputStream stream = new FileOutputStream(path)) {
            stream.write(content.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void requireMarker(String logcat, String marker, List<String> failures) {
        if (!logcat.contains(marker)) {
            failures.add("Missing required log marker: " + marker);
        }
    }

    private static void forbidMarker(String logcat, String marker, List<String> failures) {
        if (logcat.contains(marker)) {
            failures.add("Forbidden log marker was present: " + marker);
        }
    }

    private static void require(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static int parseInt(String value, int fallback) {
        if (value == null || value.isEmpty()) {
            return fallback;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }
}
