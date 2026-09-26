# Firebase Test Lab CI

Android non-VR runtime validation runs in Firebase Test Lab because GitHub-hosted runners cannot currently run the required ARM Android emulator lanes.

Required repository variables:

- `FIREBASE_TEST_LAB_PROJECT_ID`: Google Cloud/Firebase project ID.
- `FIREBASE_TEST_LAB_RESULTS_BUCKET`: Google Cloud Storage bucket name for Test Lab results, without `gs://`.
- `FIREBASE_TEST_LAB_ANDROID_DEVICE`: optional device override. The default is `model=Pixel2.arm,version=33,locale=en,orientation=landscape`.

Required repository secret:

- `FIREBASE_TEST_LAB_SERVICE_ACCOUNT_JSON`: service account JSON used by `google-github-actions/auth`.

The service account needs permission to run Firebase Test Lab matrices and write/read the configured results bucket. CI uploads the downloaded Test Lab result bundle, log-marker summary, render metrics, Markdown report, and pulled images in the Android device artifacts.
