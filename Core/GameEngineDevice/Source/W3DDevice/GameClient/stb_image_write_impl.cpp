/*
**	Command & Conquer Generals(tm)
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

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 15/10/2026 No writer that opens its own file: screenshots are written through callbacks into a
// file opened by the engine's file layer (W3DScreenshot.cpp, Common/EngineFiles.h).
#define STBI_WRITE_NO_STDIO
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

