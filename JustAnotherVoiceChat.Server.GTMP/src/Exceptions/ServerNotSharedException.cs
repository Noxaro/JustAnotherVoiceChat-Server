﻿using System;

namespace JustAnotherVoiceChat.Server.GTMP.Exceptions
{
    public class ServerNotSharedException : Exception
    {

        public ServerNotSharedException() : base("This feature is only available if the shared GtmpVoice-Server is set")
        {
            
        }
        
    }
}
