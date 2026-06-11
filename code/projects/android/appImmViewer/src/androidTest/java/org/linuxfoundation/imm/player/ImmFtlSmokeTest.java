package org.linuxfoundation.imm.player;

import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
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
public final class ImmFtlSmokeTest {
    private static final String PACKAGE_NAME = "org.linuxfoundation.imm.player";

    @Test
    public void rendersSampleFrame() throws Exception {
        Bundle args = InstrumentationRegistry.getArguments();
        String renderer = args.getString("renderer", "Vulkan");
        int waitSeconds = parseInt(args.getString("waitSeconds"), 25);

        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Context testContext = InstrumentationRegistry.getInstrumentation().getContext();

        runShell(device, "logcat -c");
        runShell(device, "am force-stop " + PACKAGE_NAME);

        Intent intent = targetContext.getPackageManager().getLaunchIntentForPackage(PACKAGE_NAME);
        assertTrue("Launch intent not found for " + PACKAGE_NAME, intent != null);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        intent.putExtra("RenderingAPI", renderer);
        targetContext.startActivity(intent);

        SystemClock.sleep(waitSeconds * 1000L);

        File artifactDir = new File(testContext.getExternalFilesDir(null), "imm-ftl");
        assertTrue("Could not create artifact directory: " + artifactDir, artifactDir.mkdirs() || artifactDir.isDirectory());

        File screenshot = new File(artifactDir, "screencap_after.png");
        assertTrue("FTL screenshot capture failed", device.takeScreenshot(screenshot));
        assertTrue("FTL screenshot is empty", screenshot.isFile() && screenshot.length() > 0L);

        String logcat = runShell(device, "logcat -d");
        writeText(new File(artifactDir, "logcat_after.txt"), logcat);

        requireMarker(logcat, "IMM Android renderer API: " + renderer);
        requireMarker(logcat, "IMMAVAL loadPath result=1");
        requireMarker(logcat, "Loaded in CPU");
        requireMarker(logcat, "Loaded in GPU");

        if ("Vulkan".equals(renderer)) {
            requireMarker(logcat, "Vulkan renderer created Android surface");
            requireMarker(logcat, "Vulkan renderer initialized with owned device");
            requireMarker(logcat, "Vulkan renderer submitted picture draw commands");
            requireMarker(logcat, "Vulkan renderer submitted static paint draw commands");
        }

        forbidMarker(logcat, "Vulkan renderer failed");
        forbidMarker(logcat, "Vulkan renderer could not");
        forbidMarker(logcat, "Vulkan draw submission is not implemented yet");
        forbidMarker(logcat, "Could not initialize piRenderer");
        forbidMarker(logcat, "Failed to load IMM");
        forbidMarker(logcat, "Fatal signal");
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
