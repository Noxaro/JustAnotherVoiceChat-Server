﻿/*
 * File: ClientPosition.cs
 * Date: 27.2.2018,
 *
 * MIT License
 *
 * Copyright (c) 2018 JustAnotherVoiceChat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

using System.Runtime.InteropServices;

namespace JustAnotherVoiceChat.Server.Wrapper.Structs
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ClientPosition
    {
        
        public float PositionX { get; }
        public float PositionY { get; }
        public float PositionZ { get; }

        public float Rotation { get; }

        public ushort Handle { get; }

        internal ClientPosition(ushort handle, float positionX, float positionY, float positionZ, float rotation)
        {
            PositionX = positionX;
            PositionY = positionY;
            PositionZ = positionZ;
            Rotation = rotation;
            Handle = handle;
        }
    }
}
