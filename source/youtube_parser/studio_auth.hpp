#pragma once
#include <string>

// Studio needs the YouTube Data API, which the main OAuth login of the app (the one of the YouTube TV client) cannot use
// (YouTube answers 403). So Studio has its own login with a Google Cloud OAuth client of the owner :
// sdmc:/3ds/IkersTube/studio_client.txt has two lines, the client id and the client secret (see Documentation/STUDIO_SETUP.md).
// The refresh token is kept in sdmc:/3ds/IkersTube/studio_tokens.txt.

bool studio_auth_has_client();
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
