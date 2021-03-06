/*
 * File: src/server.cpp
 * Date: 25.01.2018
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

#include "server.h"

#include "internal.h"
#include "client.h"
#include "log.h"

#include "../thirdparty/JustAnotherVoiceChat/include/protocol.h"

#include <future>

using namespace justAnotherVoiceChat;

Server::Server(uint16_t port, std::string teamspeakServerId, uint64_t teamspeakChannelId, std::string teamspeakChannelPassword) {
  _address.host = ENET_HOST_ANY;
  _address.port = port;

  _server = nullptr;
  _thread = nullptr;
  _clientUpdateThread = nullptr;
  _running = false;
  _distanceFactor = 1;
  _rolloffFactor = 1;

  _teamspeakServerId = teamspeakServerId;
  _teamspeakChannelId = teamspeakChannelId;
  _teamspeakChannelPassword = teamspeakChannelPassword;

  _clientConnectingCallback = nullptr;
  _clientConnectedCallback = nullptr;
  _clientRejectedCallback = nullptr;
  _clientDisconnectedCallback = nullptr;
  _clientTalkingChangedCallback = nullptr;
  _clientMicrophoneMuteChangedCallback = nullptr;
  _clientSpeakersMuteChangedCallback = nullptr;

  initialize();
}

Server::~Server() {
  close();
}

bool Server::create() {
  if (isInitialized() == false) {
    return false;
  }

  std::lock_guard<std::mutex> guard(_serverMutex);
  if (_server != nullptr) {
    return false;
  }

  _server = enet_host_create(&_address, maxClients(), NETWORK_CHANNELS, 0, 0);
  if (_server == NULL) {
    logMessage("Unable to create voice server host", LOG_LEVEL_WARNING);

    _server = nullptr;
    return false;
  }

  _running = true;
  _thread = std::make_shared<std::thread>(&Server::update, this);
  _clientUpdateThread = std::make_shared<std::thread>(&Server::updateClients, this);

  logMessage("Voice server started", LOG_LEVEL_INFO);

  return true;
}

void Server::close() {
  if (removeAllClients()) {
    // wait for clients to gracefully disconnect
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  abortThreads();

  std::lock_guard<std::mutex> guard(_serverMutex);
  if (_server != nullptr) {
    enet_host_destroy(_server);
    _server = nullptr;
  }

  logMessage("Voice server closed", LOG_LEVEL_INFO);
}

bool Server::isRunning() {
  logMessage("Locking in isRunning", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_serverMutex);
  logMessage("Locked in isRunning", LOG_LEVEL_TRACE);
  return (_server != nullptr && _running);
}

std::string Server::teamspeakServerId() const {
  return _teamspeakServerId;
}

uint64_t Server::teamspeakChannelId() const {
  return _teamspeakChannelId;
}

std::string Server::teamspeakChannelPassword() const {
  return _teamspeakChannelPassword;
}

uint16_t Server::port() const {
  return _address.port;
}

int Server::maxClients() const {
  return 256;
}

int Server::numberOfClients() const {
  return (int)_clients.size();
}

bool Server::removeClient(uint16_t gameId) {
  logMessage("Locking in removeClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in removeClient", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Client to be removed not found: " + std::to_string(gameId), LOG_LEVEL_WARNING);
    return false;
  }

  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it == client) {
      continue;
    }

    if (*it == nullptr) {
      logMessage("Nullpointer in client list", LOG_LEVEL_WARNING);
      continue;
    }
    
    (*it)->cleanupKnownClient(client);
  }

  // get client ip
  logMessage("Client disconnected " + client->endpoint(), LOG_LEVEL_INFO);

  client->disconnect();

  logMessage("Removing client from client list", LOG_LEVEL_TRACE);

  // delete client from list
  auto it = _clients.begin();
  while (it != _clients.end()) {
    if (*it == client) {
      // send callback
      if (_clientDisconnectedCallback != nullptr) {
        logMessage("Calling disconnected callback", LOG_LEVEL_TRACE);
        std::async(_clientDisconnectedCallback, client->gameId());
        logMessage("Disconnected callback called", LOG_LEVEL_TRACE);
      }

      it = _clients.erase(it);
    } else {
      it++;
    }
  }

  logMessage("Removed client " + std::to_string(gameId), LOG_LEVEL_TRACE);

  return true;
}

bool Server::removeAllClients() {
  logMessage("Locking in removeAllClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in removeAllClient", LOG_LEVEL_TRACE);

  if (_clients.empty()) {
    return false;
  }

  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it) {
      (*it)->disconnect();
    }
  }

  _clients.clear();
  return true;
}

bool Server::isClientConnected(uint16_t gameId) {
  logMessage("Locking in isClientConnected", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in isClientConnected", LOG_LEVEL_TRACE);

  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if ((*it)->gameId() == gameId) {
      return true;
    }
  }

  return false;
}

bool Server::setClientPosition(uint16_t gameId, linalg::aliases::float3 position, float rotation) {
  logMessage("Locking in setClientPosition", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in setClientPosition", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for position", LOG_LEVEL_WARNING);
    return false;
  }

  client->setPosition(position);
  client->setRotation(rotation);
  return true;
}

bool Server::setClientPositions(clientPosition_t *positionUpdates, int length) {
  logMessage("Locking in setClientPositions", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex); 
  logMessage("Locked in setClientPositions", LOG_LEVEL_TRACE);

  bool success = true;

  for (int i = 0; i < length; i++) {
    auto client = clientByGameId(positionUpdates[i].gameId);
    if (client == nullptr) {
      success = false;
      continue;
    }

    client->setPosition(linalg::aliases::float3(positionUpdates[i].x, positionUpdates[i].y, positionUpdates[i].z));
    client->setRotation(positionUpdates[i].rotation);
  }

  logMessage("Positions updated", LOG_LEVEL_TRACE);
  return success;
}

bool Server::setClientVoiceRange(uint16_t gameId, float voiceRange) {
  logMessage("Locking in setClientVoiceRange", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in setClientVoiceRange", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for voice range", LOG_LEVEL_WARNING);
    return false;
  }

  client->setVoiceRange(voiceRange);
  return true;
}

bool Server::setClientNickname(uint16_t gameId, std::string nickname) {
  logMessage("Locking in setClientNickname", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in setClientNickname", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for nickname", LOG_LEVEL_WARNING);
    return false;
  }

  client->setNickname(nickname);
  return true;
}

bool Server::setRelativePositionForClient(uint16_t listenerId, uint16_t speakerId, linalg::aliases::float3 position) {
  logMessage("Locking in setRelativePositionForClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in setRelativePositionForClient", LOG_LEVEL_TRACE);

  auto client = clientByGameId(listenerId);
  auto speaker = clientByGameId(speakerId);
  if (client == nullptr || speaker == nullptr) {
    logMessage("Unable to find client " + std::to_string(listenerId) + " for relative position", LOG_LEVEL_WARNING);
    return false;
  }

  client->addRelativeAudibleClient(speaker, position);
  return true;
}

bool Server::resetRelativePositionForClient(uint16_t listenerId, uint16_t speakerId) {
  logMessage("Locking in resetRelativePositionForClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in resetRelativePositionForClient", LOG_LEVEL_TRACE);

  auto client = clientByGameId(listenerId);
  auto speaker = clientByGameId(speakerId);
  if (client == nullptr || speaker == nullptr) {
    logMessage("Unable to find client " + std::to_string(listenerId) + " for relative position reset", LOG_LEVEL_WARNING);
    return false;
  }

  client->removeRelativeAudibleClient(speaker);
  return true;
}

bool Server::resetAllRelativePositions(uint16_t gameId) {
  logMessage("Locking in resetAllRelativePositions", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in resetAllRelativePositions", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for reset all relative positions", LOG_LEVEL_WARNING);
    return false;
  }

  client->removeAllRelativeAudibleClients();
  return true;
}

void Server::set3DSettings(float distanceFactor, float rolloffFactor) {
  _distanceFactor = distanceFactor;
  _rolloffFactor = rolloffFactor;

  // TODO: Update settings on clients
}

bool Server::muteClientForAll(uint16_t gameId, bool muted) {
  logMessage("Locking in muteClientForAll", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in muteClientForAll", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for mute client for all", LOG_LEVEL_WARNING);
    return false;
  }

  client->setMuted(muted);

  // calculate for every other client if mute changed
  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it == client || *it == nullptr) {
      continue;
    }

    if (muted) {
      // client muted, see if anybody needs to remove it's from his list
      (*it)->removeAudibleClient(client);
    } else {
      // client unmuted, see if anybody can hear him
      if (linalg::distance(client->position(), (*it)->position()) < client->voiceRange()) {
        (*it)->addAudibleClient(client);
      }
    }
  }

  return true;
}

bool Server::isClientMutedForAll(uint16_t gameId) {
  logMessage("Locking in isClientMutedForAll", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in isClientMutedForAll", LOG_LEVEL_TRACE);

  auto client = clientByGameId(gameId);
  if (client == nullptr) {
    logMessage("Unable to find client " + std::to_string(gameId) + " for is client muted", LOG_LEVEL_WARNING);
    return false;
  }

  return client->isMuted();
}

bool Server::muteClientForClient(uint16_t speakerId, uint16_t listenerId, bool muted) {
  logMessage("Locking in muteClientForClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in muteClientForClient", LOG_LEVEL_TRACE);

  auto listener = clientByGameId(listenerId);
  auto speaker = clientByGameId(speakerId);
  if (listener == nullptr || speaker == nullptr) {
    logMessage("Unable to find client " + std::to_string(speakerId) + " for mute client for client", LOG_LEVEL_WARNING);
    return false;
  }

  listener->setMutedClient(speaker, muted);

  if (muted) {
    listener->removeAudibleClient(speaker);
  } else {
    if (linalg::distance(speaker->position(), listener->position()) < speaker->voiceRange()) {
      listener->addAudibleClient(speaker);
    }
  }

  return true;
}

bool Server::isClientMutedForClient(uint16_t speakerId, uint16_t listenerId) {
  logMessage("Locking in isClientMutedForClient", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in isClientMutedForClient", LOG_LEVEL_TRACE);

  auto listener = clientByGameId(listenerId);
  auto speaker = clientByGameId(speakerId);
  if (listener == nullptr || speaker == nullptr) {
    logMessage("Unable to find client " + std::to_string(speakerId) + " for voice range", LOG_LEVEL_WARNING);
    return false;
  }

  return listener->isMutedClient(speaker);
}

void Server::registerClientConnectingCallback(ClientConnectingCallback_t callback) {
  _clientConnectingCallback = callback;
}

void Server::registerClientConnectedCallback(ClientCallback_t callback) {
  _clientConnectedCallback = callback;
}

void Server::registerClientRejectedCallback(ClientRejectedCallback_t callback) {
  _clientRejectedCallback = callback;
}

void Server::registerClientDisconnectedCallback(ClientCallback_t callback) {
  _clientDisconnectedCallback = callback;
}

void Server::registerClientTalkingChangedCallback(ClientStatusCallback_t callback) {
  _clientTalkingChangedCallback = callback;
}

void Server::registerClientSpeakersMuteChangedCallback(ClientStatusCallback_t callback) {
  _clientSpeakersMuteChangedCallback = callback;
}

void Server::registerClientMicrophoneMuteChangedCallback(ClientStatusCallback_t callback) {
  _clientMicrophoneMuteChangedCallback = callback;
}

void Server::update() {
  ENetEvent event;

  while (_running) {
    std::unique_lock<std::mutex> guard(_serverMutex);
    if (_server == nullptr) {
      return;
    }

    int code = enet_host_service(_server, &event, 1);

    guard.unlock();

    if (code > 0) {
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
          onClientConnect(event);
          break;

        case ENET_EVENT_TYPE_DISCONNECT:
          onClientDisconnect(event);
          break;

        case ENET_EVENT_TYPE_RECEIVE:
          onClientMessage(event);
          break;

        default:
          break;
      }
    } else if (code < 0) {
      logMessage("Network error: " + std::to_string(code), LOG_LEVEL_ERROR);
    }
  }

  logMessage("Update thread stopped", LOG_LEVEL_DEBUG);
}

void Server::updateClients() {
  while (_running) {
    // logMessage("Locking in updateClients", LOG_LEVEL_TRACE);
    std::unique_lock<std::mutex> guard(_clientsMutex);
    // logMessage("Locked in updateClients", LOG_LEVEL_TRACE);

    // calculate update for clients
    for (auto it = _clients.begin(); it != _clients.end(); it++) {
      // calculate update packet for this client
      auto client = *it;
      if (client == nullptr) {
        continue;
      }

      if (client->isConnected() == false) {
        logMessage("Client is not connected but in list", LOG_LEVEL_WARNING);
        continue;
      }

      for (auto clientIt = _clients.begin(); clientIt != _clients.end(); clientIt++) {
        // client to be heard
        auto audibleClient = *clientIt;
        if (audibleClient == nullptr || audibleClient == client) {
          continue;
        }

        if (client->positionChanged() == false && audibleClient->positionChanged() == false) {
          continue;
        }

        if (linalg::distance(audibleClient->position(), client->position()) < audibleClient->voiceRange()) {
          client->addAudibleClient(audibleClient);
        } else {
          client->removeAudibleClient(audibleClient);
        }
      }

      // create update packet
      client->sendUpdate();

      // send positions after audible list was updated
      client->sendPositions();
    }

    // reset all position flags
    for (auto it = _clients.begin(); it != _clients.end(); it++) {
      if (*it == nullptr) {
        continue;
      }

      (*it)->resetPositionChanged(); 
    }

    guard.unlock();

    // wait for next update
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void Server::abortThreads() {
  _running = false;

  if (_thread != nullptr) {
    if (_thread->joinable()) {
      _thread->join();
    }

    _thread = nullptr;
  }

  if (_clientUpdateThread != nullptr) {
    if (_clientUpdateThread->joinable()) {
      _clientUpdateThread->join();
    }

    _clientUpdateThread = nullptr;
  }
}

std::shared_ptr<Client> Server::clientByGameId(uint16_t gameId) const {
  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it == nullptr) {
      continue;
    }

    if ((*it)->gameId() == gameId) {
      return *it;
    }
  }

  return nullptr;
}

std::shared_ptr<Client> Server::clientByTeamspeakId(uint16_t teamspeakId) const {
  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it == nullptr) {
      continue;
    }

    if ((*it)->teamspeakId() == teamspeakId) {
      return *it;
    }
  }

  return nullptr;
}

std::shared_ptr<Client> Server::clientByPeer(ENetPeer *peer) const {
  for (auto it = _clients.begin(); it != _clients.end(); it++) {
    if (*it == nullptr) {
      continue;
    }

    if ((*it)->isPeer(peer)) {
      return *it;
    }
  }

  return nullptr;
}

void Server::onClientConnect(ENetEvent &event) {
  // get client ip
  char ip[20];
  enet_address_get_host_ip(&(event.peer->address), ip, 20);

  logMessage(std::string("New client connected ") + ip + ":" + std::to_string(event.peer->address.port), LOG_LEVEL_INFO);
}

void Server::onClientDisconnect(ENetEvent &event) {
  // get client ip
  char ip[20];
  enet_address_get_host_ip(&(event.peer->address), ip, 20);

  logMessage(std::string("Client disconnected ") + ip + ":" + std::to_string(event.peer->address.port), LOG_LEVEL_INFO);

  // remove client from list
  logMessage("Locking in onClientDisconnect", LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in onClientDisconnect", LOG_LEVEL_TRACE);

  // remove client in other's references
  auto client = clientByPeer(event.peer);
  if (client != nullptr) {
    for (auto it = _clients.begin(); it != _clients.end(); it++) {
      if (*it == client) {
        continue;
      }

      (*it)->cleanupKnownClient(client);
    }
  } else {
    logMessage("Client not found for peer on disconnect", LOG_LEVEL_WARNING);
  }

  // delete client from list
  auto it = _clients.begin();
  while (it != _clients.end()) {
    if (*it == nullptr) {
      it++;
      continue;
    }

    if ((*it)->isPeer(event.peer)) {
      // send callback
      if (_clientDisconnectedCallback != nullptr) {
        logMessage("Calling disconnected callback", LOG_LEVEL_TRACE);
        std::async(_clientDisconnectedCallback, (*it)->gameId());
        logMessage("Disconnected callback called", LOG_LEVEL_TRACE);
      }

      it = _clients.erase(it);
    } else {
      it++;
    }
  }

  logMessage("Client removed on disconnect", LOG_LEVEL_DEBUG);
}

void Server::onClientMessage(ENetEvent &event) {
  // handle protocol check and handshake message before anything else
  if (event.channelID == NETWORK_PROTOCOL_CHANNEL) {
    handleProtocolMessage(event);
    enet_packet_destroy(event.packet);

    return;
  } else if (event.channelID == NETWORK_HANDSHAKE_CHANNEL) {
    handleHandshake(event);
    enet_packet_destroy(event.packet);

    return;
  }

  // get client for message
  logMessage("Locking in onClientMessage " + std::to_string(event.channelID), LOG_LEVEL_TRACE);
  std::lock_guard<std::mutex> guard(_clientsMutex);
  logMessage("Locked in onClientMessage", LOG_LEVEL_TRACE);

  auto client = clientByPeer(event.peer);
  if (client == nullptr) {
    enet_packet_destroy(event.packet);
    return;
  }

  switch (event.channelID) {
    case NETWORK_STATUS_CHANNEL:
      bool talkingChanged;
      bool microphoneChanged;
      bool speakersChanged;

      if (client->handleStatus(event.packet, &talkingChanged, &microphoneChanged, &speakersChanged)) {
        // status changed, call callbacks
        if (talkingChanged && _clientTalkingChangedCallback != nullptr) {
          logMessage("Calling talking callback", LOG_LEVEL_TRACE);
          std::async(_clientTalkingChangedCallback, client->gameId(), client->isTalking());
          logMessage("Talking callback called", LOG_LEVEL_TRACE);
        }

        if (microphoneChanged && _clientMicrophoneMuteChangedCallback != nullptr) {
          logMessage("Calling mic callback", LOG_LEVEL_TRACE);
          std::async(_clientMicrophoneMuteChangedCallback, client->gameId(), client->hasMicrophoneMuted());
          logMessage("Mic callback called", LOG_LEVEL_TRACE);
        }

        if (speakersChanged && _clientSpeakersMuteChangedCallback != nullptr) {
          logMessage("Calling speaker callback", LOG_LEVEL_TRACE);
          std::async(_clientSpeakersMuteChangedCallback, client->gameId(), client->hasSpeakersMuted());
          logMessage("Speaker callback called", LOG_LEVEL_TRACE);
        }
      }
      break;

    default:
      logMessage("Unhandled message received", LOG_LEVEL_WARNING);
      break;
  }

  enet_packet_destroy(event.packet);
}

void Server::handleProtocolMessage(ENetEvent &event) {
  protocolPacket_t protocolPacket;

  std::string data((char *)event.packet->data, event.packet->dataLength);
  std::istringstream is(data);

  try {
    cereal::BinaryInputArchive archive(is);
    archive(protocolPacket);
  } catch (std::exception &e) {
    logMessage(e.what(), LOG_LEVEL_ERROR);
    return;
  }

  // compare protocol versions
  bool clientMatches = verifyProtocolVersion(protocolPacket.versionMajor, protocolPacket.versionMinor, PROTOCOL_MIN_VERSION_MAJOR, PROTOCOL_MIN_VERSION_MINOR);
  bool serverMatches = verifyProtocolVersion(PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR, protocolPacket.minimumVersionMajor, protocolPacket.minimumVersionMinor);

  if (clientMatches == false || serverMatches == false) {
    int disconnectStatus;

    if (clientMatches == false) {
      logMessage("Client uses an outdated protocol version: " + std::to_string(protocolPacket.versionMajor) + "." + std::to_string(protocolPacket.versionMinor), LOG_LEVEL_WARNING);

      disconnectStatus = DISCONNECT_STATUS_OUTDATED_CLIENT;
    } else {
      logMessage("Server uses an outdated protocol version: " + std::to_string(PROTOCOL_VERSION_MAJOR) + "." + std::to_string(PROTOCOL_VERSION_MINOR), LOG_LEVEL_WARNING);

      disconnectStatus = DISCONNECT_STATUS_OUTDATED_SERVER;
    }

    sendProtocolResponse(event.peer, STATUS_CODE_OUTDATED_PROTOCOL_VERSION);

    enet_peer_disconnect_later(event.peer, disconnectStatus);
    return;
  }

  sendProtocolResponse(event.peer, STATUS_CODE_OK);
}

void Server::handleHandshake(ENetEvent &event) {
  handshakePacket_t handshakePacket;

  std::string data((char *)event.packet->data, event.packet->dataLength);
  std::istringstream is(data);

  try {
    cereal::BinaryInputArchive archive(is);
    archive(handshakePacket);
  } catch (std::exception &e) {
    logMessage(e.what(), LOG_LEVEL_ERROR);
    return;
  }

  if (handshakePacket.statusCode != STATUS_CODE_OK) {
    logMessage("Handshake error: " + std::to_string(handshakePacket.statusCode), LOG_LEVEL_INFO);

    enet_peer_disconnect(event.peer, 0);

    if (_clientRejectedCallback != nullptr) {
      logMessage("Calling rejected callback", LOG_LEVEL_TRACE);
      std::async(_clientRejectedCallback, handshakePacket.gameId, handshakePacket.statusCode);
      logMessage("Rejected callback called", LOG_LEVEL_TRACE);
    }
    return;
  }

  // send teamspeak information if no teamspeak id is known
  if (handshakePacket.teamspeakId == 0) {
    // send handshake response
    sendHandshakeResponse(event.peer, STATUS_CODE_OK, "OK");
  } else {
    // search for already existing client
    logMessage("Locking in handleHandshake", LOG_LEVEL_TRACE);
    std::unique_lock<std::mutex> guard(_clientsMutex);
    logMessage("Locked in handleHandshake", LOG_LEVEL_TRACE);

    auto client = clientByPeer(event.peer);
    if (client != nullptr) {
      logMessage("Client with that peer is already in list", LOG_LEVEL_WARNING);
      return;
    }

    guard.unlock();

    logMessage("Calling connecting callback", LOG_LEVEL_TRACE);
    if (_clientConnectingCallback != nullptr) {
      std::future<bool> result = std::async(_clientConnectingCallback, handshakePacket.gameId, handshakePacket.teamspeakClientUniqueIdentity.c_str());
      if (result.get() == false) {
        enet_peer_disconnect(event.peer, DISCONNECT_STATUS_REJECTED);
        return;
      }
    }
    logMessage("Connecting callback called", LOG_LEVEL_TRACE);

    // save new client in list
    logMessage("Locking in handleHandshake", LOG_LEVEL_TRACE);
    guard.lock();
    logMessage("Locked in handleHandshake", LOG_LEVEL_TRACE);

    client = std::make_shared<Client>(event.peer, handshakePacket.gameId, handshakePacket.teamspeakId);
    _clients.push_back(client);

    guard.unlock();

    if (_clientConnectedCallback != nullptr) {
      logMessage("Calling connected callback", LOG_LEVEL_TRACE);
      std::async(_clientConnectedCallback, handshakePacket.gameId);
      logMessage("Connecting callback called", LOG_LEVEL_TRACE);
    }

    logMessage("New client established " + std::to_string(client->gameId()) + " " + std::to_string(client->teamspeakId()), LOG_LEVEL_INFO);
  }
}

void Server::sendHandshakeResponse(ENetPeer *peer, int statusCode, std::string reason) {
  handshakeResponsePacket_t packet;
  packet.statusCode = statusCode;
  packet.reason = reason;
  packet.teamspeakServerUniqueIdentifier = _teamspeakServerId;
  packet.channelId = _teamspeakChannelId;
  packet.channelPassword = _teamspeakChannelPassword;

  // serialize packet
  std::ostringstream os;

  try {
    cereal::BinaryOutputArchive archive(os);
    archive(packet);
  } catch (std::exception &e) {
    logMessage(e.what(), LOG_LEVEL_ERROR);
    return;
  }

  // send packet
  auto data = os.str();

  ENetPacket *rawPacket = enet_packet_create(data.c_str(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, NETWORK_HANDSHAKE_CHANNEL, rawPacket);
}

void Server::sendProtocolResponse(ENetPeer *peer, int statusCode) {
  protocolResponsePacket_t packet;
  packet.statusCode = statusCode;
  packet.versionMajor = PROTOCOL_VERSION_MAJOR;
  packet.versionMinor = PROTOCOL_VERSION_MINOR;

  // serialize packet
  std::ostringstream os;

  try {
    cereal::BinaryOutputArchive archive(os);
    archive(packet);
  } catch (std::exception &e) {
    logMessage(e.what(), LOG_LEVEL_ERROR);
    return;
  }

  // send packet
  auto data = os.str();

  ENetPacket *rawPacket = enet_packet_create(data.c_str(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, NETWORK_PROTOCOL_CHANNEL, rawPacket);
}
