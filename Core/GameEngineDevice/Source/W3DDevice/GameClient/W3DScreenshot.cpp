/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "W3DDevice/GameClient/W3DScreenshot.h"
#include "Common/GlobalData.h"
#include "GameClient/GameText.h"
#include "GameClient/InGameUI.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/surfaceclass.h"
#include "WWLib/mpsc_intrusive_queue.h"
#include <stb_image_write.h>
#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 15/10/2026 A screenshot is written into a file opened through the engine's file layer
// (Common/EngineFiles.h).
#include "Common/EngineFiles.h"
#endif

#if RTS_ENGINE_CONTEXT
#include "Common/EngineThread.h" // GeneralsX @feature cemlyn007 15/10/2026 rts::startEngineThread
#endif
#ifndef _WIN32
#if !RTS_ENGINE_CONTEXT
#include <thread>
#endif
#include <filesystem>
#endif

struct ScreenshotThreadData
{
	ScreenshotThreadData()
		: pixelData(nullptr)
	{
	}

	~ScreenshotThreadData()
	{
		delete[] pixelData;
	}

	unsigned char* pixelData; // is owner
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	bool is16Bit;
	char userDataDirectory[_MAX_PATH];
	char leafname[_MAX_FNAME];
	int quality;
	ScreenshotFormat format;
};

// TheInGameUI is not thread safe, so the screenshot threads cannot show the success message
// themselves. Each thread pushes the written filename onto this queue and the main thread
// shows all pending messages in W3D_UpdateScreenshotMessages, so no message is lost when
// multiple screenshot threads finish within the same frame.
struct ScreenshotWrittenMessage
{
	ScreenshotWrittenMessage* next;
	char leafname[_MAX_FNAME];
};
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(MPSCIntrusiveQueue<ScreenshotWrittenMessage>, s_screenshotWrittenQueue);
#define s_screenshotWrittenQueue RTS_ENGINE_GLOBAL_REF(s_screenshotWrittenQueue)
#else
static MPSCIntrusiveQueue<ScreenshotWrittenMessage> s_screenshotWrittenQueue;
#endif

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 15/10/2026 Started through the engine's ThreadStarter (Common/EngineThread.h), which owns
// `data`. The work is short and waits for nothing, so the thread checks for a stop request before each part of it.
static void screenshotThreadFunc(ScreenshotThreadData* data, const std::stop_token& stop)
{
	if (stop.stop_requested())
		return;
#elif defined(_WIN32)
static DWORD WINAPI screenshotThreadFunc(LPVOID param)
{
	ScreenshotThreadData* data = static_cast<ScreenshotThreadData*>(param);
#else
static void screenshotThreadFunc(ScreenshotThreadData* data)
{
#endif

	// TheSuperHackers @feature bobtista 08/07/2026 Save screenshots into a Screenshots subfolder
	// to keep the user data root folder tidy.
	char pathname[_MAX_PATH];
	strlcpy(pathname, data->userDataDirectory, ARRAY_SIZE(pathname));
	strlcat(pathname, "Screenshots\\", ARRAY_SIZE(pathname));
#ifdef _WIN32
	CreateDirectory(pathname, nullptr);
#else
	std::error_code ec;
	std::filesystem::create_directory(pathname, ec);
#endif
	strlcat(pathname, data->leafname, ARRAY_SIZE(pathname));

	const unsigned int width = data->width;
	const unsigned int height = data->height;

	// Convert to R8G8B8 for stb_image_write.
	unsigned char* image = new unsigned char[3 * width * height];
#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 24/10/2026 The image is owned here, so that a stop request that ends one of the
	// thread's waits with rts::EngineThreadStopped (Common/EngineThread.h) frees it too.
	const std::unique_ptr<unsigned char[]> ownedImage(image);
#endif

	if (!data->is16Bit)
	{
		// Convert A8R8G8B8/X8R8G8B8 to R8G8B8
		for (unsigned int y = 0; y < height; ++y)
		{
			const unsigned int* srcLine = reinterpret_cast<const unsigned int*>(data->pixelData + y * data->pitch);
			for (unsigned int x = 0; x < width; ++x)
			{
				const unsigned int argb = srcLine[x];
				const unsigned int index = 3 * (x + y * width);
				image[index + 0] = (unsigned char)(argb >> 16); // r
				image[index + 1] = (unsigned char)(argb >> 8);  // g
				image[index + 2] = (unsigned char)(argb >> 0);  // b
			}
		}
	}
	else
	{
		// Convert R5G6B5 to R8G8B8
		for (unsigned int y = 0; y < height; ++y)
		{
			const unsigned short* srcLine = reinterpret_cast<const unsigned short*>(data->pixelData + y * data->pitch);
			for (unsigned int x = 0; x < width; ++x)
			{
				const unsigned short rgb = srcLine[x];
				const unsigned int index = 3 * (x + y * width);
				image[index + 0] = (unsigned char)((rgb & 0xF800) >> 8); // r
				image[index + 1] = (unsigned char)((rgb & 0x07E0) >> 3); // g
				image[index + 2] = (unsigned char)((rgb & 0x001F) << 3); // b
			}
		}
	}

	int success = 0;
#if RTS_ENGINE_CONTEXT
	if (stop.stop_requested())
		return;
	if (FILE* const file = rts::openEngineFile(pathname, "wb"))
	{
		const auto writeToFile = [](void* context, void* bytes, int size) {
			fwrite(bytes, 1, static_cast<size_t>(size), static_cast<FILE*>(context));
		};
		switch (data->format)
		{
			case SCREENSHOT_JPEG:
				success = stbi_write_jpg_to_func(writeToFile, file, width, height, 3, image, data->quality);
				break;
			case SCREENSHOT_PNG:
				success = stbi_write_png_to_func(writeToFile, file, width, height, 3, image, width * 3);
				break;
		}
		if (ferror(file))
			success = 0;
		if (rts::closeEngineFile(file) != 0)
			success = 0;
	}
#else
	switch (data->format)
	{
		case SCREENSHOT_JPEG:
			success = stbi_write_jpg(pathname, width, height, 3, image, data->quality);
			break;
		case SCREENSHOT_PNG:
			success = stbi_write_png(pathname, width, height, 3, image, width * 3);
			break;
	}
#endif

	if (success)
	{
#if RTS_ENGINE_CONTEXT
		// GeneralsX @feature cemlyn007 24/10/2026 The queue, a per-engine static, is reached before the message is made,
		// so that a stop request that ends the wait for it leaks nothing.
		MPSCIntrusiveQueue<ScreenshotWrittenMessage>& writtenQueue = s_screenshotWrittenQueue;
		ScreenshotWrittenMessage* message = new ScreenshotWrittenMessage;
		strlcpy(message->leafname, data->leafname, ARRAY_SIZE(message->leafname));
		writtenQueue.Push(message);
#else
		ScreenshotWrittenMessage* message = new ScreenshotWrittenMessage;
		strlcpy(message->leafname, data->leafname, ARRAY_SIZE(message->leafname));
		s_screenshotWrittenQueue.Push(message);
#endif
	}
	else
	{
		DEBUG_LOG(("Failed to write screenshot %s", pathname));
	}

#if !RTS_ENGINE_CONTEXT
	delete[] image;
	delete data;
#endif

#if defined(_WIN32) && !RTS_ENGINE_CONTEXT
	return success;
#endif
}

void W3D_UpdateScreenshotMessages()
{
	ScreenshotWrittenMessage* message = s_screenshotWrittenQueue.Flush();
	while (message != nullptr)
	{
		UnicodeString ufileName;
		ufileName.translate(message->leafname);
		TheInGameUI->message(TheGameText->fetch("GUI:ScreenCapture"), ufileName.str());
		ScreenshotWrittenMessage* next = message->next;
		delete message;
		message = next;
	}
}

void W3D_TakeCompressedScreenshot(ScreenshotFormat format, Int jpegQuality)
{
	static constexpr const char* const ScreenshotFormatExtensions[] = { "jpg", "png" };
	static_assert(ARRAY_SIZE(ScreenshotFormatExtensions) == SCREENSHOT_FORMAT_COUNT, "Incorrect array size");

	// The filename is created here so the timestamp matches the capture time.
	char leafname[_MAX_FNAME];
	const char* extension = ScreenshotFormatExtensions[format];

	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf(leafname, "sshot_%04d%02d%02d_%02d%02d%02d_%03d.%s",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, extension);

	// TheSuperHackers @bugfix xezon 21/05/2025 Get the back buffer and create a copy of the surface.
	// Originally this code took the front buffer and tried to lock it. This does not work when the
	// render view clips outside the desktop boundaries. It crashed the game.
	SurfaceClass* surface = DX8Wrapper::_Get_DX8_Back_Buffer();
	SurfaceClass::SurfaceDescription surfaceDesc;
	surface->Get_Description(surfaceDesc);

	// TheSuperHackers @bugfix bobtista 08/07/2026 Support the 16 bit back buffer format that the
	// game uses when running in 16 bit color mode. Reading it with the 32 bit stride read garbage.
	const bool is32Bit = surfaceDesc.Format == WW3D_FORMAT_A8R8G8B8 || surfaceDesc.Format == WW3D_FORMAT_X8R8G8B8;
	const bool is16Bit = surfaceDesc.Format == WW3D_FORMAT_R5G6B5;

	if (!is32Bit && !is16Bit)
	{
		DEBUG_LOG(("Screenshot does not support back buffer format %d", (int)surfaceDesc.Format));
		surface->Release_Ref();
		return;
	}

	SurfaceClass* surfaceCopy = NEW_REF(SurfaceClass, (DX8Wrapper::_Create_DX8_Surface(surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format)));
	DX8Wrapper::_Copy_DX8_Rects(surface->Peek_D3D_Surface(), nullptr, 0, surfaceCopy->Peek_D3D_Surface(), nullptr);

	surface->Release_Ref();
	surface = nullptr;

	struct Rect
	{
		int Pitch;
		void* pBits;
	} lrect;

	lrect.pBits = surfaceCopy->Lock(&lrect.Pitch);
	if (lrect.pBits == nullptr)
	{
		surfaceCopy->Release_Ref();
		return;
	}

	ScreenshotThreadData* threadData = new ScreenshotThreadData();
	threadData->width = surfaceDesc.Width;
	threadData->height = surfaceDesc.Height;
	threadData->pitch = lrect.Pitch;
	threadData->is16Bit = is16Bit;
	threadData->quality = jpegQuality;
	threadData->format = format;
	strlcpy(threadData->userDataDirectory, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(threadData->userDataDirectory));
	strlcpy(threadData->leafname, leafname, ARRAY_SIZE(threadData->leafname));

	// Copy the locked surface with a single memcpy, including any row padding. The pixel
	// conversion and all file operations are done on the screenshot thread to keep the
	// main thread cheap.
	threadData->pixelData = new unsigned char[threadData->pitch * threadData->height];
	memcpy(threadData->pixelData, lrect.pBits, threadData->pitch * threadData->height);

	surfaceCopy->Unlock();
	surfaceCopy->Release_Ref();
	surfaceCopy = nullptr;

#if RTS_ENGINE_CONTEXT
	const std::shared_ptr<ScreenshotThreadData> ownedThreadData(threadData);
	rts::startEngineThread("screenshot", [ownedThreadData](std::stop_token stop) {
		screenshotThreadFunc(ownedThreadData.get(), stop);
	});
#elif defined(_WIN32)
	const HANDLE hThread = CreateThread(nullptr, 0, screenshotThreadFunc, threadData, 0, nullptr);
	if (hThread)
	{
		CloseHandle(hThread);
	}
	else
	{
		delete threadData;
	}
#else
	std::thread t(screenshotThreadFunc, threadData);
	t.detach();
#endif
}
