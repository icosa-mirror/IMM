/**
 * **********************************************************************************
 *
 * Filename : QuillPlayerVrActivity.java Content : Created : Authors :
 *
 * Copyright : Copyright 2014 Oculus VR, LLC. All Rights reserved.
 */
package org.linuxfoundation.imm.player

import android.Manifest
import android.app.NativeActivity
import android.content.Intent
import android.net.Uri
import android.content.pm.PackageManager
import android.graphics.*
import android.os.AsyncTask
import android.os.Bundle
import android.util.Log
import androidx.annotation.Keep
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.io.File
import kotlinx.coroutines.*

@Keep
class MainActivity : NativeActivity(), CoroutineScope by CoroutineScope(Dispatchers.IO) {
  private var didFinishInit = false

  companion object {
    val TAG = MainActivity::class.java.simpleName
    const val EXTRA_QUILL_PATH = "QUILL_PATH"
    const val EXTRA_QUILL_NAME = "QUILL_NAME"
    const val EXTRA_QUILL_ARTIST_NAME = "QUILL_ARTIST_NAME"
    const val EXTRA_QUILL_CREATION_TIME = "QUILL_CREATION_TIME"
    const val EXTRA_QUILL_TRACKING_LEVEL = "QUILL_TRACKING_LEVEL"
    const val EXTRA_QUILL_PLAYER_SPAWN_LOCATION =
        "QUILL_PLAYER_SPAWN_LOCATION" // Default, Spaces_Player1, Spaces_Player2, Spaces_Player3,
    // Spaces_Player4
    const val EXTRA_QUILL_EYE_BUFFER_SCALE = "QUILL_EYE_BUFFER_SCALE"
    const val EXTRA_QUILL_RENDERING_TECHNIQUE = "QUILL_RENDERING_TECHNIQUE"
    const val EXTRA_RENDERING_API = "RenderingAPI"
    const val EXTRA_VALIDATION_RENDER_WIDTH = "ValidationRenderWidth"
    const val EXTRA_VALIDATION_RENDER_HEIGHT = "ValidationRenderHeight"
    const val EXTRA_VALIDATION_FIXED_DT = "ValidationFixedDt"
    const val EXTRA_VALIDATION_PLAYER_FRAME = "ValidationPlayerFrame"
    const val PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE = 0x1

    @JvmStatic external fun nativeSetAssetDirectory(assetsDir: String?)
    @JvmStatic external fun nativeSetExternalFilesDirectory(externalDir: String?)
    @JvmStatic external fun nativeSendMessage(message: String?, messageType: Int)
    @JvmStatic external fun nativeSetQuillRenderingTechnique(renderingTechnique: Int)
    @JvmStatic external fun nativeSetRenderingApi(renderingApi: String?)
    @JvmStatic external fun nativeSetValidationRenderSize(width: Int, height: Int)
    @JvmStatic external fun nativeSetValidationPlayback(fixedDt: Double, playerFrame: Long)
    @JvmStatic
    external fun nativeSetEyeBufferScale(
        scaleFactor: Float
    ) // scale factor to be applied to default eye buffers
    @JvmStatic external fun nativeSetPlayerSpawnLocation(spawnLocation: String?)
    @JvmStatic external fun nativeSetTrackingTransformLevel(trackingTransformLevel: String?)

    @JvmStatic
    fun logIntentExtras(intent: Intent) {
      val extras = intent.extras
      if (extras != null) {
        Log.d(TAG, "found extras!")
        for (key in extras.keySet()) {
          val value = extras[key]
          Log.d(TAG, String.format("  %s %s (%s)", key, value.toString(), value?.javaClass?.name))
        }
      }
    }

    fun humanReadableByteCount(bytes: Long): String {
      val unit = 1024
      if (bytes < unit) return "$bytes B"
      val exp = (Math.log(bytes.toDouble()) / Math.log(unit.toDouble())).toInt()
      val pre = "kMGTPE"[exp - 1].toString() + ""
      return String.format("%.1f %sB", bytes / Math.pow(unit.toDouble(), exp.toDouble()), pre)
    }

    init {
      Log.d(TAG, "Loading library libappImmPlayer.so")
      System.loadLibrary("appImmPlayer")
      System.setProperty("kotlinx.coroutines.debug", if (BuildConfig.DEBUG) "on" else "off")
    }
  }

  override fun onCreate(savedInstanceState: Bundle?) {
    applyRenderingApiExtra(intent)
    applyValidationRenderSizeExtra(intent)
    applyValidationPlaybackExtra(intent)
    super.onCreate(savedInstanceState)

    if (intent != null)
        Log.d(TAG, "onCreate intent category: " + intent.categories + " action: " + intent.action)
    else Log.d(TAG, "onCreate null intent")

    finishInit()
  }

  private fun finishInit() {
    if (didFinishInit) {
      return
    }
    didFinishInit = true

    Utils.assetsDirectory = applicationContext.filesDir.path + "/"
    Utils.cacheDirectory = filesDir.path + "/cache/"
    val cacheDir = File(Utils.cacheDirectory)
    if (!cacheDir.exists()) {
      cacheDir.mkdir()
    }
    nativeSetAssetDirectory(Utils.assetsDirectory)
    val externalDir = getExternalFilesDir(null)?.path
    nativeSetExternalFilesDirectory(externalDir)
    
    // Extract assets synchronously to avoid race condition
    val shouldReload = Utils.shouldReloadAssets()
    extractAssetSync(Utils.quillDemoPath, shouldReload)
    extractAssetSync(Utils.quillErrorPath, shouldReload)
    
    // Verify the demo file exists
    val demoFile = File(Utils.assetsDirectory, Utils.quillDemoPath)
    if (!demoFile.exists()) {
      Log.e(TAG, "DEMO FILE NOT FOUND: ${demoFile.absolutePath}")
    } else {
      Log.d(TAG, "Demo file ready: ${demoFile.absolutePath}, size: ${demoFile.length()}")
    }

    if (!handleNewIntent(intent)) {
      val externalImmPath = findExternalImmPath()
      if (externalImmPath != null) {
        Log.d(TAG, "Loading IMM from external files: $externalImmPath")
        loadImm(externalImmPath)
      } else {
        Log.d(TAG, "No intent or external IMM path; native loader will select content")
      }
    }
  }
  
  private fun extractAssetSync(assetName: String, forceExtract: Boolean) {
    val assetPath = Utils.assetsDirectory + assetName
    val file = File(assetPath)
    
    if (!forceExtract && file.exists()) {
      Log.d(TAG, "$assetPath already exists, size: ${file.length()}")
      return
    }
    
    if (file.exists()) {
      file.delete()
    }
    
    Log.d(TAG, "Extracting $assetName to $assetPath")
    try {
      file.parentFile?.mkdirs()
      resources.assets.open(assetName).use { input ->
        file.outputStream().use { output ->
          input.copyTo(output)
        }
      }
      Log.d(TAG, "Successfully extracted $assetName, size: ${file.length()}")
    } catch (e: Exception) {
      Log.e(TAG, "Failed to extract $assetName: $e")
    }
  }

  private fun findExternalImmPath(): String? {
    val baseDir = getExternalFilesDir(null) ?: return null
    val immDir = File(baseDir, "IMM")
    if (!immDir.exists()) {
      return null
    }

    val defaultImm = File(immDir, "default.imm")
    if (defaultImm.exists()) {
      return defaultImm.absolutePath
    }

    val defaultAuthoring = File(immDir, "default")
    if (defaultAuthoring.exists()) {
      return defaultAuthoring.absolutePath
    }

    val immFiles = immDir.listFiles { file ->
      file.isFile && file.name.endsWith(".imm", ignoreCase = true)
    } ?: return null

    if (immFiles.isEmpty()) {
      return null
    }

    val newest = immFiles.maxByOrNull { it.lastModified() } ?: return null
    return newest.absolutePath
  }

  override fun onNewIntent(intent: Intent) {
    super.onNewIntent(intent)
    Log.d(
        TAG,
        "onNewIntent intent package: ${intent.`package`} scheme: ${intent.scheme} type: ${intent.type} category: ${intent.categories} action: ${intent.action} data: ${intent.dataString} flags: ${intent.flags}")
    handleNewIntent(intent)
  }

  override fun onResume() {
    super.onResume()

    if (intent != null)
        Log.d(
            TAG,
            "onResume intent  package: ${intent.`package`} scheme: ${intent.scheme} type: ${intent.type} category: ${intent.categories} action: ${intent.action} data: ${intent.dataString} flags: ${intent.flags}")
    else Log.d(TAG, "onResume null intent")
  }

  override fun onDestroy() {
    cancel()

    super.onDestroy()
    Log.d(TAG, "onDestroy")
  }

  override fun onPause() {
    Log.d(TAG, "onPause")
    super.onPause()
  }

  private fun handleNewIntent(intent: Intent): Boolean {

    logIntentExtras(intent)

    // Dev Gallery uses intent extras to launch Quillustrations
    if (handleIntentExtras(intent)) {
      return true
    }

    if (handleViewIntent(intent)) {
      return true
    }

    return false
  }

  private fun handleViewIntent(intent: Intent): Boolean {
    if (Intent.ACTION_VIEW != intent.action) {
      return false
    }

    val data = intent.data ?: return false
    val uriPath = when (data.scheme) {
      "file" -> importUriToInternalFiles(data)
      "content" -> importUriToInternalFiles(data)
      else -> null
    }

    if (uriPath.isNullOrEmpty()) {
      return false
    }

    loadImmPath(uriPath)
    return true
  }

  private fun importUriToInternalFiles(uri: Uri): String? {
    try {
      val baseDir = filesDir
      val immDir = File(baseDir, "IMM")
      if (!immDir.exists()) {
        immDir.mkdirs()
      }

      val name = uri.lastPathSegment?.substringAfterLast('/')?.ifEmpty { null }
      val fileName = if (name != null && name.endsWith(".imm", ignoreCase = true)) {
        name
      } else {
        "imported_${System.currentTimeMillis()}.imm"
      }

      val destFile = File(immDir, fileName)
      contentResolver.openInputStream(uri)?.use { input ->
        destFile.outputStream().use { output ->
          input.copyTo(output)
        }
      } ?: return null

      Log.d(TAG, "Imported IMM to internal files: ${destFile.absolutePath}")
      return destFile.absolutePath
    } catch (e: Exception) {
      Log.e(TAG, "Failed to import IMM from URI: $uri", e)
      return null
    }
  }

  private fun handleIntentExtras(intent: Intent): Boolean {
    // Check if intent was launched with the Quill disk path
    val extras = intent.extras
    if (extras != null) { // Should be either eye or floor
      if (extras.containsKey(EXTRA_QUILL_TRACKING_LEVEL)) {
        val level = extras.getString(EXTRA_QUILL_TRACKING_LEVEL, "")
        if (!level.isEmpty()) {
          Log.d(TAG, "found tracking level extra $level")
          nativeSetTrackingTransformLevel(level)
        }
      }
      if (extras.containsKey(EXTRA_QUILL_EYE_BUFFER_SCALE)) {
        val scaleFactor = extras.getFloat(EXTRA_QUILL_EYE_BUFFER_SCALE)
        Log.d(TAG, "found eye buffer scale extra $scaleFactor")
        if (scaleFactor <= 0) {
          Log.e(TAG, "invalid eye buffer scale: $scaleFactor")
        } else {
          nativeSetEyeBufferScale(scaleFactor)
        }
      }
      if (extras.containsKey(EXTRA_QUILL_RENDERING_TECHNIQUE)) {
        val renderingTechnique = extras.getInt(EXTRA_QUILL_RENDERING_TECHNIQUE)
        if (renderingTechnique >= 0) {
          Log.d(TAG, "found renderingTechnique extra $renderingTechnique")
          nativeSetQuillRenderingTechnique(renderingTechnique)
        }
      }
      applyRenderingApiExtra(intent)
      applyValidationRenderSizeExtra(intent)
      applyValidationPlaybackExtra(intent)
      if (extras.containsKey(EXTRA_QUILL_PLAYER_SPAWN_LOCATION)) {
        val spawnLocation = extras.getString(EXTRA_QUILL_PLAYER_SPAWN_LOCATION)
        Log.d(TAG, "found player spawn location extra $spawnLocation")
        nativeSetPlayerSpawnLocation(spawnLocation)
      }
      // getInt failed, so parsing integer from string here
      if (extras.containsKey(EXTRA_QUILL_PATH)) {
        val path = extras.getString(EXTRA_QUILL_PATH)
        path?.let {
          loadImmPath(it)
          return true
        }
      }
    }
    return false
  }

  private fun applyRenderingApiExtra(intent: Intent?) {
    val extras = intent?.extras ?: return
    if (!extras.containsKey(EXTRA_RENDERING_API)) {
      return
    }

    val renderingApi = extras.getString(EXTRA_RENDERING_API, "")
    if (renderingApi.isNotEmpty()) {
      Log.d(TAG, "found rendering API extra $renderingApi")
      nativeSetRenderingApi(renderingApi)
    }
  }

  private fun applyValidationRenderSizeExtra(intent: Intent?) {
    val extras = intent?.extras ?: return
    if (!extras.containsKey(EXTRA_VALIDATION_RENDER_WIDTH) || !extras.containsKey(EXTRA_VALIDATION_RENDER_HEIGHT)) {
      return
    }

    val width = extras.getInt(EXTRA_VALIDATION_RENDER_WIDTH, 0)
    val height = extras.getInt(EXTRA_VALIDATION_RENDER_HEIGHT, 0)
    if (width > 0 && height > 0) {
      Log.d(TAG, "found validation render size ${width}x${height}")
      nativeSetValidationRenderSize(width, height)
    }
  }

  private fun applyValidationPlaybackExtra(intent: Intent?) {
    val extras = intent?.extras ?: return
    if (!extras.containsKey(EXTRA_VALIDATION_FIXED_DT) && !extras.containsKey(EXTRA_VALIDATION_PLAYER_FRAME)) {
      return
    }

    val fixedDt = when (val value = extras.get(EXTRA_VALIDATION_FIXED_DT)) {
      is Double -> value
      is Float -> value.toDouble()
      is String -> value.toDoubleOrNull() ?: -1.0
      else -> -1.0
    }
    val playerFrame = when (val value = extras.get(EXTRA_VALIDATION_PLAYER_FRAME)) {
      is Long -> value
      is Int -> value.toLong()
      is String -> value.toLongOrNull() ?: -1L
      else -> -1L
    }
    if (fixedDt >= 0.0 || playerFrame >= 0L) {
      Log.d(TAG, "found validation playback fixedDt=$fixedDt playerFrame=$playerFrame")
      nativeSetValidationPlayback(fixedDt, playerFrame)
    }
  }

  private fun loadImmPath(path: String) {
    Log.d(TAG, "Loading IMM: $path")
    loadImm(path)
  }

  private fun loadImm(path: String) {
    nativeSendMessage(path, Message.LoadImmPath.value)
  }

  private fun loadErrorImm(message: Message) {
    nativeSetTrackingTransformLevel("floor")
    nativeSetPlayerSpawnLocation("Default")
    nativeSetQuillRenderingTechnique(PaintRenderingTechnique.Static.value)

    if (message == Message.Disconnected) {
      nativeSendMessage(Imm.Error.value, message.value)
    } else {
      nativeSendMessage(Imm.Error.value, message.value)
    }
  }

  private fun unpackQuillAssets() {
    val shouldReload = Utils.shouldReloadAssets()

    Utils.ExtractAssetsTask(this, Utils.quillErrorPath, null, shouldReload)
        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)

    Utils.ExtractAssetsTask(this, Utils.quillDemoPath, null, shouldReload)
        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)

    // Extract uiLib data.
    /*val controllersLoadTimer = Utils.AssetLoadTimer(
        TimingLogger(TAG, "controller asset extraction"),
        Utils.controllerAssets.size
    )
    for (controllerAsset in Utils.controllerAssets) {
        Utils.ExtractAssetsTask(this, controllerAsset, controllersLoadTimer, shouldReload)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)
    }*/

    // We should only ovewrite the asset version file here after all assets from Gallery and for
    // player have been loaded.
    val forceExtract = true
    Utils.ExtractAssetsTask(this, Utils.assetVersionFile, null, forceExtract)
        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)
  }

  private fun requestExternalStoragePermission(): Boolean {
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) !=
        PackageManager.PERMISSION_GRANTED) {

      ActivityCompat.requestPermissions(
          this,
          arrayOf(
              Manifest.permission.READ_EXTERNAL_STORAGE,
              Manifest.permission.WRITE_EXTERNAL_STORAGE),
          PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE)
      return false
    }
    return true
  }

  override fun onRequestPermissionsResult(
      requestCode: Int,
      permissions: Array<String>,
      grantResults: IntArray
  ) {
    if (requestCode != PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE) {
      return
    }
    if (grantResults.size <= 0 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
      Log.e(TAG, "Access not granted for reading local Quill media in external storage")
    }
    finishInit()
  }
}
