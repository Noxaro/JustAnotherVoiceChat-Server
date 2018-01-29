﻿using AlternateVoice.Server.GTMP.Factories;
using AlternateVoice.Server.GTMP.Interfaces;
using GrandTheftMultiplayer.Server.API;
using GrandTheftMultiplayer.Server.Constant;
using GrandTheftMultiplayer.Server.Elements;
using GrandTheftMultiplayer.Shared.Gta.Tasks;

namespace AlternateVoice.Server.GTMP.Resource
{
    public partial class VoiceScript : Script
    {

        private readonly IGtmpVoiceServer _voiceServer;

        public VoiceScript()
        {
            _voiceServer = GtmpVoice.CreateServer(API, "game.alternate-life.de", 23332, 23);

            _voiceServer.OnServerStarted += () =>
            {
                API.consoleOutput(LogCat.Info, $"GtmpVoiceServer started, listening on {_voiceServer.Hostname}:{_voiceServer.Port}");
            };
            
            _voiceServer.OnServerStopping += () =>
            {
                API.consoleOutput(LogCat.Info, "GtmpVoiceServer stopping!");
            };

            _voiceServer.OnClientPrepared += c =>
            {
                c.Player.triggerEvent("VOICE_SET_HANDSHAKE", true, c.HandshakeUrl);
            };
            
            _voiceServer.OnClientConnected += c =>
            {
                c.Player.triggerEvent("VOICE_SET_HANDSHAKE", false);
            };
            
            _voiceServer.OnClientDisconnected += c =>
            {
                c.Player.triggerEvent("VOICE_SET_HANDSHAKE", true, c.HandshakeUrl);
            };

            _voiceServer.OnPlayerStartsTalking += OnPlayerStartsTalking;
            _voiceServer.OnPlayerStopsTalking += OnPlayerStopsTalking;

            API.onClientEventTrigger += OnClientEventTrigger;

            _voiceServer.Start();
            
            API.onResourceStop += OnResourceStop;
        }

        private void OnClientEventTrigger(Client sender, string eventName, params object[] arguments)
        {
            if (eventName != "VOICE_ROTATION")
            {
                return;
            }

            var rotation = (float)arguments[0];
            var voiceClient = _voiceServer.GetVoiceClientOfPlayer(sender);

            if (voiceClient == null)
            {
                return;
            }

            voiceClient.CameraRotation = rotation;
        }

        private void OnPlayerStartsTalking(IGtmpVoiceClient speakingClient)
        {
            API.playPlayerAnimation(speakingClient.Player, (int)(AnimationFlag.Loop | AnimationFlag.AllowRotation), "mp_facial", "mic_chatter");
            // Needs further investigation of animation handling 
            //API.sendNativeToPlayersInRange(speakingClient.Player.position, 300f, Hash.TASK_PLAY_ANIM, speakingClient.Player.handle, "mp_facial", "mic_chatter",
            //  8f, -4f, -1, (int)(AnimationFlag.Loop | AnimationFlag.AllowRotation), 0.0f, false, false, false);
        }

        private void OnPlayerStopsTalking(IGtmpVoiceClient speakingClient)
        {
            // Stopping all animations?
            API.stopPlayerAnimation(speakingClient.Player);
            // Needs further investigation of animation handling 
            // API.sendNativeToPlayersInRange(speakingClient.Player.position, 300f, Hash.TASK_PLAY_ANIM, speakingClient.Player.handle, "mp_facial", "mic_chatter1",
            //    8f, -4f, -1, (int)(AnimationFlag.Loop | AnimationFlag.AllowRotation), 0.0f, false, false, false);
        }

        private void OnResourceStop()
        {
            _voiceServer.Stop();
            _voiceServer.Dispose();
        }
    }
}
