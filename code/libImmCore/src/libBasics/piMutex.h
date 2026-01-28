//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#pragma once

namespace ImmCore
{

	class piMutex
	{
	public:
		bool Init(void);
		void End(void);
		void Lock(void);
		void UnLock(void);
	private:
		void * p = 0;
	};


} // namespace ImmCore
