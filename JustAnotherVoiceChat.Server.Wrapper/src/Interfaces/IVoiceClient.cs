﻿/*
 * File: IVoiceClient.cs
 * Date: 21.2.2018,
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

using System;
using System.Collections.Generic;
using JustAnotherVoiceChat.Server.Wrapper.Math;
using JustAnotherVoiceChat.Server.Wrapper.Structs;

namespace JustAnotherVoiceChat.Server.Wrapper.Interfaces
{
    public interface IVoiceClient : IDisposable
    {
        
        VoiceHandle Handle { get; }
        
        bool Connected { get; }
        
        bool Microphone { get; }
        
        bool Speakers { get; }
        
        Vector3 Position { get; }
        float CameraRotation { get; set; }

        string HandshakeUrl { get; }

        bool SetNickname(string nickname);
        bool SetVoiceRange(float range);
        
        bool SetListeningPosition(Vector3 position, float rotation);
        bool SetListeningPositions(List<ClientPosition> clientPositions);
        bool SetRelativeSpeakerPosition(IVoiceClient speaker, Vector3 position);
        bool ResetRelativeSpeakerPosition(IVoiceClient speaker);
        bool ResetAllRelativeSpeakerPositions();

        bool MuteForAll(bool muted);
        bool IsMutedForAll();
        bool MuteSpeaker(IVoiceClient speaker, bool muted);
        bool IsSpeakerMuted(IVoiceClient speaker);
    }
}
