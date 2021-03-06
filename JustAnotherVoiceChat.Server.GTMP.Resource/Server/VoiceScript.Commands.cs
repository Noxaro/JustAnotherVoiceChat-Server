﻿/*
 * File: VoiceScript.Commands.cs
 * Date: 22.2.2018,
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

using GrandTheftMultiplayer.Server.Elements;
using GrandTheftMultiplayer.Server.Managers;
using GrandTheftMultiplayer.Shared;
using GrandTheftMultiplayer.Shared.Math;
using JustAnotherVoiceChat.Server.GTMP.Extensions;

namespace JustAnotherVoiceChat.Server.GTMP.Resource
{
    public partial class VoiceScript
    {
        [Command("range")]
        public void PlayerVoiceRange(Client sender, float range)
        {
            sender.SetVoiceRange(range);
        }

        [Command("call")]
        public void CallPlayer(Client sender, Client target)
        {
            if (sender == target)
            {
                sender.sendChatMessage("I don't think that would be smart...");
                return;
            }
            
            if (_phoneHandler.StartCall(sender, target))
            {
                sender.sendChatMessage($"Calling {target.name}...");
                target.sendChatMessage($"Incoming call from {sender.name}! Accept the call with ~g~/answer true");
            }
            else
            {
                sender.sendChatMessage($"Can't call {target.name}! Please try again later.");
            }
        }

        [Command("answer")]
        public void AnswerCall(Client sender, bool decision)
        {
            if (!_phoneHandler.AnswerCall(sender, decision, out var caller))
            {
                return;
            }

            if (decision)
            {
                sender.sendChatMessage($"You accepted the call with {caller.name}! You can talk now.");
                caller.sendChatMessage($"{sender.name} accepted your call! You can talk now.");
            }
            else
            {
                sender.sendChatMessage($"You denied the call with {caller.name}! Maybe someone else?");
                caller.sendChatMessage($"{sender.name} denied your call! :(");
            }
        }

        [Command("hangup")]
        public void HangupCall(Client sender)
        {
            if (!_phoneHandler.HangupCall(sender, out var opponent))
            {
                return;
            }
            
            sender.sendChatMessage($"You stopped the call with {opponent.name}!");
            opponent.sendChatMessage($"{sender.name} has stopped the call!");
        }

        [Command("mute")]
        public void MuteMe(Client sender, bool status)
        {
            sender.MuteForAll(status);
            sender.sendChatMessage($"You have been {(status ? "muted" : "unmuted")} for all.");
        }

        [Command("muteplayer")]
        public void MutePlayer(Client sender, Client speaker, bool status)
        {
            sender.MuteSpeaker(speaker, status);
            sender.sendChatMessage($"You {(status ? "muted" : "unmuted")} {speaker.name} locally.");
        }

        [Command("enableRadio", Alias = "ER")]
        public void EnableRadio(Client sender, string channel)
        {
            if (channel == null)
            {
                return;
            }

            _radioHandler.EnableRadio(sender, channel);
            sender.sendChatMessage("Radio ~g~ON");
        }

        [Command("disableRadio", Alias = "DR")]
        public void DisableRadio(Client sender)
        {
            _radioHandler.DisableRadio(sender);
            sender.sendChatMessage("Radio ~r~OFF");
        }

        [Command("useRadio", Alias = "UR")]
        public void UseRadio(Client sender, string channel, bool decision)
        {
            _radioHandler.UseRadio(sender, channel, decision);
            if (decision == true)
            {
                sender.sendChatMessage("~g~ Radio activ");
            }
            else
            {
                sender.sendChatMessage("~r~ Radio inactiv");
            }
        }

        [Command("veh")]
        public void CreateVehicle(Client sender, VehicleHash vehicleHash)
        {
            var vehicle = API.createVehicle(vehicleHash, sender.position, sender.rotation, 0, 0);
            API.setPlayerIntoVehicle(sender, vehicle, -1);
        }
    }
}
