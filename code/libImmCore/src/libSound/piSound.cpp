//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include "piSoundEngineNULL.h"
#if defined(WINDOWS)
#include "windows/piSoundEngineDS.h"
#include "windows/piSoundEngineAudioSDKBackend.h"
#elif defined(ANDROID) || defined(__ANDROID__)
#include "android/piSoundEngineAndroid.h"
#endif

namespace ImmCore
{
	piSoundEngineBackend * piCreateSoundEngineBackend(piSoundEngineBackend::API api, piLog *log)
	{
		piSoundEngineBackend *me = nullptr;
		if (api == piSoundEngineBackend::API::Null) me = new piSoundEngineBackendNULL();
#if defined(WINDOWS)
		else if (api == piSoundEngineBackend::API::DirectSound)    me = new piSoundEngineBackendDS();
		else if (api == piSoundEngineBackend::API::DirectSoundOVR) me = new piSoundEngineAudioSDKBackend();
#elif defined(ANDROID) || defined(__ANDROID__)
		else if (api == piSoundEngineBackend::API::Android)        me = new piSoundEngineBackendAndroid();
#endif
		else return nullptr;

		if (!me->doInit(log))
			return nullptr;

		return me;
	}

	void piDestroySoundEngineBackend(piSoundEngineBackend *me)
	{
		me->doDeinit();
		delete me;
	}

}
