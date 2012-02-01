/*
 * libaudiodecoder - Native Portable Audio Decoder Library
 * libaudiodecoder API Header File
 * Latest version available at: http://www.oscillicious.com/libaudiodecoder
 *
 * Copyright (c) 2010-2012 Albert Santoni, Bill Good, RJ Ryan  
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire libaudiodecoder license; however, 
 * the Oscillicious community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

#include <string>
#include <iostream>
#include "audiodecodercoreaudio.h"


AudioDecoderCoreAudio::AudioDecoderCoreAudio(const std::string& filename) 
: AudioDecoderBase(filename)
, m_headerFrames(0)
{
    m_filename = filename;
}

AudioDecoderCoreAudio::~AudioDecoderCoreAudio() {
	ExtAudioFileDispose(m_audioFile);

}

int AudioDecoderCoreAudio::open() {
    std::cerr << "AudioDecoderCoreAudio::open()" << std::endl;

    //Open the audio file.
    OSStatus err;

    /** This code blocks works with OS X 10.5+ only. DO NOT DELETE IT for now. */
    /*CFStringRef urlStr = CFStringCreateWithCharacters(0,
   				reinterpret_cast<const UniChar *>(
                //qurlStr.unicode()), qurlStr.size());
                m_filename.data()), m_filename.size());
                */
    CFStringRef urlStr = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                   m_filename.c_str(), 
                                                   CFStringGetSystemEncoding());

    CFURLRef urlRef = CFURLCreateWithFileSystemPath(NULL, urlStr, kCFURLPOSIXPathStyle, false);
    err = ExtAudioFileOpenURL(urlRef, &m_audioFile);
    CFRelease(urlStr);
    CFRelease(urlRef);

    /** TODO: Use FSRef for compatibility with 10.4 Tiger. 
        Note that ExtAudioFileOpen() is deprecated above Tiger, so we must maintain
        both code paths if someone finishes this part of the code.
    FSRef fsRef;
    CFURLGetFSRef(reinterpret_cast<CFURLRef>(url.get()), &fsRef);
    err = ExtAudioFileOpen(&fsRef, &m_audioFile);
    */

	if (err != noErr)
	{
        std::cerr << "AudioDecoderCoreAudio: Error opening file." << std::endl;
		return AUDIODECODER_ERROR;
	}

    // get the input file format
    CAStreamBasicDescription inputFormat;
    UInt32 size = sizeof(inputFormat);
    m_inputFormat = inputFormat;
    err = ExtAudioFileGetProperty(m_audioFile, kExtAudioFileProperty_FileDataFormat, &size, &inputFormat);
	if (err != noErr)
	{
        std::cerr << "AudioDecoderCoreAudio: Error getting file format." << std::endl;
		return AUDIODECODER_ERROR;
	}    
    
	// create the output format
	CAStreamBasicDescription outputFormat;
    bzero(&outputFormat, sizeof(AudioStreamBasicDescription));
	outputFormat.mFormatID = kAudioFormatLinearPCM;
	outputFormat.mSampleRate = inputFormat.mSampleRate;
	outputFormat.mChannelsPerFrame = 2;
    outputFormat.mFormatFlags = kAudioFormatFlagsCanonical;  
    //kAudioFormatFlagsCanonical means Native endian, float, packed on Mac OS X, 
    //but signed int for iOS instead.

    //Note iPhone/iOS only supports signed integers supposedly:
    //outputFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
	
    //Debugging:
    //printf ("Source File format: "); inputFormat.Print();
    //printf ("Dest File format: "); outputFormat.Print();


	/*
	switch(inputFormat.mBitsPerChannel) {
		case 16:
			outputFormat.mFormatFlags =  kAppleLosslessFormatFlag_16BitSourceData;
			break;
		case 20:
			outputFormat.mFormatFlags =  kAppleLosslessFormatFlag_20BitSourceData;
			break;
		case 24:
			outputFormat.mFormatFlags =  kAppleLosslessFormatFlag_24BitSourceData;
			break;
		case 32:
			outputFormat.mFormatFlags =  kAppleLosslessFormatFlag_32BitSourceData;
			break;
	}*/

    // get and set the client format - it should be lpcm
    CAStreamBasicDescription clientFormat = outputFormat; //We're always telling the OS to do the conversion to floats for us now
	clientFormat.mChannelsPerFrame = 2;
	clientFormat.mBytesPerFrame = sizeof(SAMPLE)*clientFormat.mChannelsPerFrame;
	clientFormat.mBitsPerChannel = sizeof(SAMPLE)*8; //16 for signed int, 32 for float;
	clientFormat.mFramesPerPacket = 1;
	clientFormat.mBytesPerPacket = clientFormat.mBytesPerFrame*clientFormat.mFramesPerPacket;
	clientFormat.mReserved = 0;
	m_clientFormat = clientFormat;
    size = sizeof(clientFormat);
    
    err = ExtAudioFileSetProperty(m_audioFile, kExtAudioFileProperty_ClientDataFormat, size, &clientFormat);
	if (err != noErr)
	{
		//qDebug() << "SSCA: Error setting file property";
        std::cerr << "AudioDecoderCoreAudio: Error setting file property." << std::endl;
		return AUDIODECODER_ERROR;
	}
	
	//Set m_iChannels and m_iNumSamples;
	m_iChannels = clientFormat.NumberChannels();

	//get the total length in frames of the audio file - copypasta: http://discussions.apple.com/thread.jspa?threadID=2364583&tstart=47
	UInt32		dataSize;
	SInt64		totalFrameCount;		
	dataSize	= sizeof(totalFrameCount); //XXX: This looks sketchy to me - Albert
	err			= ExtAudioFileGetProperty(m_audioFile, kExtAudioFileProperty_FileLengthFrames, &dataSize, &totalFrameCount);
	if (err != noErr)
	{
        std::cerr << "AudioDecoderCoreAudio: Error getting number of frames." << std::endl;
		return AUDIODECODER_ERROR;
	}

      //
      // WORKAROUND for bug in ExtFileAudio
      //
      
      AudioConverterRef acRef;
      UInt32 acrsize=sizeof(AudioConverterRef);
      err = ExtAudioFileGetProperty(m_audioFile, kExtAudioFileProperty_AudioConverter, &acrsize, &acRef);
      //_ThrowExceptionIfErr(@"kExtAudioFileProperty_AudioConverter", err);

      AudioConverterPrimeInfo primeInfo;
      UInt32 piSize=sizeof(AudioConverterPrimeInfo);
      err = AudioConverterGetProperty(acRef, kAudioConverterPrimeInfo, &piSize, &primeInfo);
      if(err != kAudioConverterErr_PropertyNotSupported) // Only if decompressing
      {
         //_ThrowExceptionIfErr(@"kAudioConverterPrimeInfo", err);
         
         m_headerFrames=primeInfo.leadingFrames;
      }
	
	m_iNumSamples = (totalFrameCount/*-m_headerFrames*/)*m_iChannels;
	m_fDuration = m_iNumSamples / static_cast<float>(inputFormat.mSampleRate * m_iChannels);
	m_iSampleRate = inputFormat.mSampleRate;
	
	//Seek to position 0, which forces us to skip over all the header frames.
	//This makes sure we're ready to just let the Analyser rip and it'll
	//get the number of samples it expects (ie. no header frames).
	seek(0);

    return AUDIODECODER_OK;
}

long AudioDecoderCoreAudio::seek(long filepos) {
    OSStatus err = noErr;
    SInt64 segmentStart = filepos / 2;

      err = ExtAudioFileSeek(m_audioFile, (SInt64)segmentStart+m_headerFrames);
      //_ThrowExceptionIfErr(@"ExtAudioFileSeek", err);
	//qDebug() << "SSCA: Seeking to" << segmentStart;

	//err = ExtAudioFileSeek(m_audioFile, filepos / 2);		
	if (err != noErr)
	{
		//qDebug() << "SSCA: Error seeking to" << filepos;// << GetMacOSStatusErrorString(err) << GetMacOSStatusCommentString(err);
        std::cerr << "AudioDecoderCoreAudio: Error seeking to sample " << filepos << std::endl;
	}

    m_iPositionInSamples = filepos;

    return filepos;
}

unsigned int AudioDecoderCoreAudio::read(unsigned long size, const SAMPLE *destination) {
    OSStatus err;
    SAMPLE *destBuffer(const_cast<SAMPLE*>(destination));
    unsigned int samplesWritten = 0;
    unsigned int i = 0;
    UInt32 numFrames = 0;
    unsigned int totalFramesToRead = size/2;
    unsigned int numFramesRead = 0;
    unsigned int numFramesToRead = totalFramesToRead;

    while (numFramesRead < totalFramesToRead) { 
    	numFramesToRead = totalFramesToRead - numFramesRead;
    	
		AudioBufferList fillBufList;
		fillBufList.mNumberBuffers = 1; //Decode a single track
        //See CoreAudioTypes.h for definitins of these variables:
		fillBufList.mBuffers[0].mNumberChannels = m_inputFormat.mChannelsPerFrame;
		fillBufList.mBuffers[0].mDataByteSize = numFramesToRead*2 * sizeof(SAMPLE);
		fillBufList.mBuffers[0].mData = (void*)(&destBuffer[numFramesRead*2]);
			
        // client format is always linear PCM - so here we determine how many frames of lpcm
        // we can read/write given our buffer size
		numFrames = numFramesToRead; //This silly variable acts as both a parameter and return value.
		err = ExtAudioFileRead (m_audioFile, &numFrames, &fillBufList);
		//The actual number of frames read also comes back in numFrames.
		//(It's both a parameter to a function and a return value. wat apple?)
		//XThrowIfError (err, "ExtAudioFileRead");	
		if (!numFrames) {
				// this is our termination condition
			break;
		}
		numFramesRead += numFrames;
    }
    
    m_iPositionInSamples += numFramesRead*m_iChannels;

    std::cout << "Read samples " << destBuffer[0] << " " << destBuffer[1] << std::endl;

    return numFramesRead*m_iChannels;
}

/*
int AudioDecoderCoreAudio::parseHeader() {
    if (getFilename().endsWith(".m4a"))
        setType("m4a");
    else if (getFilename().endsWith(".mp3"))
        setType("mp3");
    else if (getFilename().endsWith(".mp2"))
        setType("mp2");

    bool result = false;

    if (getType() == "m4a") {
        TagLib::MP4::File f(getFilename().toUtf8().constData());
        result = processTaglibFile(f);
        TagLib::MP4::Tag* tag = f.tag();
        if (tag) {
            processMP4Tag(tag);
        }
    } else if (getType() == "mp3") {
		TagLib::MPEG::File f(getFilename().toUtf8().constData());

        // Takes care of all the default metadata
        result = processTaglibFile(f);

        // Now look for MP3 specific metadata (e.g. BPM)
        TagLib::ID3v2::Tag* id3v2 = f.ID3v2Tag();
        if (id3v2) {
            processID3v2Tag(id3v2);
        }

        TagLib::APE::Tag *ape = f.APETag();
        if (ape) {
            processAPETag(ape);
        }
    } else if (getType() == "mp2") {
        //TODO: MP2 metadata. Does anyone use mp2 files anymore?
        //      Feels like 1995 again...
    }


    if (result)
        return AUDIODECODER_OK;
    return AUDIODECODER_ERROR;
}
*/


// static
std::vector<std::string> AudioDecoderCoreAudio::supportedFileExtensions() {
    std::vector<std::string> list;
    list.push_back(std::string("m4a"));
    list.push_back(std::string("mp3"));
    list.push_back(std::string("mp2"));
    //Can add mp3, mp2, ac3, and others here if you want.
    //See:
    //  http://developer.apple.com/library/mac/documentation/MusicAudio/Reference/AudioFileConvertRef/Reference/reference.html#//apple_ref/doc/c_ref/AudioFileTypeID
    
    return list;
}

