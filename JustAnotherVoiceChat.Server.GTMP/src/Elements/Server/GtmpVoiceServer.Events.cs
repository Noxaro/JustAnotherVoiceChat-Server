﻿/*
 * File: GtmpVoiceServer.Events.cs
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

using GrandTheftMultiplayer.Server.Constant;
using GrandTheftMultiplayer.Server.Elements;
using JustAnotherVoiceChat.Server.GTMP.Interfaces;
using JustAnotherVoiceChat.Server.Wrapper.Delegates;
using JustAnotherVoiceChat.Server.Wrapper.Enums;

namespace JustAnotherVoiceChat.Server.GTMP.Elements.Server
{
    public partial class GtmpVoiceServer
    {

        public new event Delegates<IGtmpVoiceClient>.EmptyEvent OnServerStarted;
        public new event Delegates<IGtmpVoiceClient>.EmptyEvent OnServerStopping;
        
        public event GtmpVoiceDelegates.GtmpVoiceClientEvent OnClientPrepared;
        public new event GtmpVoiceDelegates.GtmpVoiceLogMessageEvent OnLogMessage;

        private void AttachToEvents()
        {
            _api.onPlayerFinishedDownload += OnPlayerConnect;
            _api.onPlayerDisconnected += OnPlayerDisconnect;

            base.OnServerStarted += OnVoiceServerStarted;
            base.OnServerStopping += OnVoiceServerStopping;
            base.OnLogMessage += OnVoiceServerLogMessage;
        }

        private void OnVoiceServerLogMessage(string message, LogLevel logLevel)
        {
            LogCat logCat;
            switch (logLevel)
            {
                case LogLevel.Trace:
                    logCat = LogCat.Trace;
                    break;
                case LogLevel.Debug:
                    logCat = LogCat.Debug;
                    break;
                case LogLevel.Warning:
                    logCat = LogCat.Warn;
                    break;
                case LogLevel.Error:
                    logCat = LogCat.Error;
                    break;
                default:
                    logCat = LogCat.Info;
                    break;
            }
            
            OnLogMessage?.Invoke(logCat, message);
        }

        private void OnVoiceServerStarted()
        {
            foreach (var player in _api.getAllPlayers())
            {
                RegisterPlayer(player);
            }
            
            OnServerStarted?.Invoke();
        }

        private void OnVoiceServerStopping()
        {
            foreach (var client in GetClients())
            {
                UnregisterPlayer(client.Player);
            }
            
            OnServerStopping?.Invoke();
        }

        private void OnPlayerConnect(Client player)
        {
            if (RegisterPlayer(player) != null)
            {
                return;
            }

            player.kick("Failed to start VoiceClient!");
            _api.consoleOutput(LogCat.Error, $"Failed to start VoiceClient for Client {player.name}");
        }

        private void OnPlayerDisconnect(Client player, string reason)
        {
            UnregisterPlayer(player);
        }
    }
}
