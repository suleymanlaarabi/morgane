#pragma once

#include <string>

namespace game {

/** Resource: the server address typed on the menu and the state of its connection. */
struct Session {
    std::string address = "127.0.0.1:4242";
    bool connecting = false;
    bool connected = false;
    bool in_game = false;  // the Game scene is open, maybe under pause or options
    bool steering = false; // the Game scene is on top: keys steer the ship
    std::string error;     // why the last attempt or connection ended, shown on the menu
};

/** Start connecting to Session::address (`host[:port]`). On a malformed address, sets
 * Session::error and returns false. */
bool join(Session &session);

/** Short messages stacked at the top of the screen for a few seconds, above every scene. */
void notify(std::string text);

/** Module: tracks the connection in Session and announces departures. Import after net::Client
 * and game::register_network(). */
struct Sessions {
    static void import(const Session &session);
};

} // namespace game
