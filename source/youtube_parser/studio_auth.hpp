#pragma once
#include <string>

// Studio needs the YouTube Data API, which the main OAuth login of the app (the one of the YouTube TV client) cannot use
// (YouTube answers 403). So Studio has its own login with a Google Cloud OAuth client of the owner :
// The client comes with the app (build secrets) or is set up in the Studio tab (typed or scanned from a QR); it is kept in
// sdmc:/3ds/IkersTube/studio_client.txt (two lines : client id, client secret). See Documentation/STUDIO_SETUP.md.
// The refresh token is kept in sdmc:/3ds/IkersTube/studio_tokens.txt.

bool studio_auth_has_client();
bool studio_auth_client_built_in(); // the app was built with the client (nothing to set up)
// saves a client set up on this console (typed or scanned from a QR) : returns false if something is missing
bool studio_auth_set_client(const std::string &id, const std::string &secret);
bool studio_auth_signed_in();
std::string studio_auth_access_token(); // refreshed when it expired, "" if there is no Studio login
void studio_auth_sign_out();

struct StudioDeviceCode {
	std::string user_code, url, device_code;
	int interval = 5;
	int expires_in = 0;
};
bool studio_auth_request_code(StudioDeviceCode &out, std::string &error);
// 1 : signed in, 0 : waiting for the user, -1 : failed or expired (see `error`)
int studio_auth_poll(const StudioDeviceCode &code, std::string &error);

// the token the Data API calls use : the Studio login if there is one, otherwise the main login (which YouTube refuses)
std::string studio_bearer_token();
