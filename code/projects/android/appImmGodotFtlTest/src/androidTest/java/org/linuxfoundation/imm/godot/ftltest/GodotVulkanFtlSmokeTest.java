package org.linuxfoundation.imm.godot.ftltest;

import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

@RunWith(AndroidJUnit4.class)
public final class GodotVulkanFtlSmokeTest {
    private static final String PACKAGE_NAME = "org.linuxfoundation.imm.godot.sample";
    private static final String ACTIVITY_NAME = "com.godot.game.GodotApp";
    private static final String DEVICE_CAPTURE_NAME = "device_after_smoke.png";

    @Test
    public void rendersGodotVulkanSampleFrame() throws Exception {
        Bundle args = InstrumentationRegistry.getArguments();
        int waitSeconds = parseInt(args.getString("waitSeconds"), 60);

        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File artifactDir = new File(targetContext.getExternalFilesDir(null), "imm-ftl");
        assertTrue("Could not create artifact directory: " + artifactDir, artifactDir.mkdirs() || artifactDir.isDirectory());
        File logcatPath = new File(artifactDir, "logcat_after.txt");
        File capturePath = new File(artifactDir, DEVICE_CAPTURE_NAME);

        runShell(device, "logcat -c");
        runShell(device, "am force-stop " + PACKAGE_NAME);
        runShell(device, "am start -n " + PACKAGE_NAME + "/" + ACTIVITY_NAME);

        String logcat = waitForSmoke(device, waitSeconds);
        writeText(logcatPath, logcat);
        assertTrue("FTL screenshot capture failed", device.takeScreenshot(capturePath));
        runShell(device, "am force-stop " + PACKAGE_NAME);

        requireMarker(logcat, "IMM Godot Vulkan visual smoke passed");
        requireMarker(logcat, "Loaded in CPU");
        requireMarker(logcat, "Loaded in GPU");
        requireMarker(logcat, "Vulkan renderer submitted picture draw commands");
        requireMarker(logcat, "Vulkan renderer submitted static paint draw commands");
        requireMarker(logcat, "last_vulkan_frame_started");

        forbidMarker(logcat, "Fatal signal");
        forbidMarker(logcat, "ImmViewerNode setup failed");
        forbidMarker(logcat, "ImmViewerCompositorEffect setup failed");
        forbidMarker(logcat, "visual smoke failures");

        assertTrue("Godot diagnostic device capture missing: " + capturePath, capturePath.isFile());
        assertTrue("Godot diagnostic device capture is empty: " + capturePath, capturePath.length() > 0L);
    }

    private static String waitForSmoke(UiDevice device, int waitSeconds) throws Exception {
        long deadline = SystemClock.elapsedRealtime() + waitSeconds * 1000L;
        String logcat = "";
        while (SystemClock.elapsedRealtime() < deadline) {
            SystemClock.sleep(1000L);
            logcat = runShell(device, "logcat -d");
            if (logcat.contains("IMM Godot Vulkan visual smoke passed")) {
                return logcat;
            }
            if (logcat.contains("Fatal signal") || logcat.contains("visual smoke failures")) {
                return logcat;
            }
        }
        return runShell(device, "logcat -d");
    }

    private static String runShell(UiDevice device, String command) throws Exception {
        return device.executeShellCommand(command);
    }

    private static void writeText(File path, String content) throws Exception {
        try (FileOutputStream stream = new FileOutputStream(path)) {
            stream.write(content.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void requireMarker(String logcat, String marker) {
        assertTrue("Missing required log marker: " + marker, logcat.contains(marker));
    }

    private static void forbidMarker(String logcat, String marker) {
        assertTrue("Forbidden log marker was present: " + marker, !logcat.contains(marker));
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
