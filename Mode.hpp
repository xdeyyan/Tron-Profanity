#ifndef HPP_MODE
#define HPP_MODE

#include <string>
#include <vector>

#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

class Mode {
	private:
		Mode();

	public:
		static Mode matching(const std::string fileName);
		
		std::vector<cl_uchar> data1;
		std::vector<cl_uchar> data2;
		cl_uchar score;
		cl_uchar prefixCount;
		cl_uchar suffixCount;
		cl_uchar matchingCount;
};

#endif /* HPP_MODE */
