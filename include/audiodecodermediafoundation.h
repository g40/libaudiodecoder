/**
 * \file audiodecodermediafoundation.h
 * \class AudioDecoderMediaFoundation
 * \brief Decodes MPEG4/AAC audio using the SourceReader interface of the
 * Media Foundation framework included in Windows 7.
 * \author Bill Good <bkgood at gmail dot com>
 * \author Albert Santoni <alberts at mixxx dot org>
 * \date Jan 10, 2011
 */


#ifndef AUDIODECODERMEDIAFOUNDATION_H
#define AUDIODECODERMEDIAFOUNDATION_H

#include "audiodecoderbase.h"

#ifdef _MAKE_DLL
//Force MSVC to generate a .lib file with /implib but without a .def file
//http://msdn.microsoft.com/en-us/library/67wc07b9(v=vs.80).aspx
DllExport int AudioDecoderMediaFoundation = 1;
#endif

struct IMFSourceReader;
struct IMFMediaType;
struct IMFMediaSource;

class DllExport AudioDecoderMediaFoundation : public AudioDecoderBase
{
	static const int bufferSize = 8192;

public:
	AudioDecoderMediaFoundation();
	~AudioDecoderMediaFoundation();
	//
	int open(const std::string& filename);
	//
	bool close();
	//
	int seek(int sampleIdx);
	// read N frames into buffer
	int read(int frames, const float* buffer);
	// read with interleave
	int read(int frames, std::vector<float*>& buffer);
	//
	int samples();
	std::vector<std::string> supportedFileExtensions();

private:
	bool configureAudioStream();
	bool readProperties();
	void copyFrames(short* dest, size_t* destFrames, const short* src, size_t srcFrames);
	int read_internal(int size);
	inline double secondsFromMF(__int64 mf);
	inline __int64 mfFromSeconds(double sec);
	inline __int64 frameFromMF(__int64 mf);
	inline __int64 mfFromFrame(__int64 frame);
	IMFSourceReader* m_pReader;
	IMFMediaType* m_pAudioType;
	std::wstring m_wfilename;
	int m_nextFrame;

	__int64 m_mfDuration;
	long m_iCurrentPosition;
	bool m_dead;
	bool m_seeking;
	unsigned int m_iBitsPerSample;

	// JME audit:vector?
	short* m_leftoverBuffer;
	size_t m_leftoverBufferSize;
	size_t m_leftoverBufferLength;
	int m_leftoverBufferPosition;

	// 'native' bits. no conversions applied. interleaved
	// JME audit: needs 24/32bit support
	// JME audit: needs dynamic allocation
	short m_destBuffer[bufferSize];
};

#endif // ifndef AUDIODECODERMEDIAFOUNDATION_H
