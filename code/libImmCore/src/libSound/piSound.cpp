//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include "piSoundEngineNULL.h"
#include "piSoundEngineMiniaudio.h"

#if defined(WINDOWS) && !defined(IMM_NULL_AUDIO)
#include "windows/piSoundEngineDS.h"
#include "windows/piSoundEngineAudioSDKBackend.h"
#endif

namespace ImmCore
{
	piSoundEngineBackend * piCreateSoundEngineBackend(piSoundEngineBackend::API api, piLog *log)
	{
		piSoundEngineBackend *me = nullptr;
		if (api == piSoundEngineBackend::API::Null)
		{
			me = new piSoundEngineBackendNULL();
		}
		else if (api == piSoundEngineBackend::API::Miniaudio)
		{
			me = new piSoundEngineBackendMiniaudio();
		}
#if defined(WINDOWS) && !defined(IMM_NULL_AUDIO)
		else if (api == piSoundEngineBackend::API::DirectSound)
		{
			me = new piSoundEngineBackendDS();
		}
		else if (api == piSoundEngineBackend::API::DirectSoundOVR)
		{
			me = new piSoundEngineAudioSDKBackend();
		}
#endif
		else
		{
			return nullptr;
		}

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
