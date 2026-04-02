#include <Exports.hpp>
#include <cstdio>
#include <cstring>
#include <string>

#include <curl/curl.h>
#include <json.h>

namespace {
size_t WriteToString(void* contents, size_t size, size_t nmemb, void* userp) {
    const auto total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<const char*>(contents), total);
    return total;
}

bool PostKeyToServer(const char* key, std::string& response) {
    // Not sure if this is correct, will test on Disobey infra IG
    constexpr const char* Endpoint = "https://exceptional.dissi.fi:8000/check";

    CURL* curl = curl_easy_init();
    if(!curl) {
        return false;
    }

    std::string payload = std::string("{\"key\":\"") + key + "\"}";

    curl_easy_setopt(curl, CURLOPT_URL, Endpoint);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const auto res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}
} // namespace

int main(int argc, char** argv) {
    if(argc < 3) {
        std::printf("Usage: %s --key key\n", argv[0]);
        return 1;
    }

    if(strncmp(argv[1], "--key", 5ull) != 0) {
        std::printf("Usage: %s --key key\n", argv[0]);
        return 1;
    }

    if(!CheckLicenseKey(argv[2])) {
        std::puts("Wrong key!");
        return 1;
    }

    std::string response;
    if(!PostKeyToServer(argv[2], response)) {
        std::puts("Remote check failed.");
        return 2;
    }

    // This can leak, it's fine
    auto json = json_parse(response.c_str(), response.size());

    if(!json || json->type != json_type_object) {
        std::puts("Failed to parse response");
        return 2;
    }

    auto obj = json_value_as_object(json);

    if(obj->length != 2) {
        std::puts("Invalid server response!");
        return 3;
    }

    for(auto i = obj->start; i; i = i->next) {
        if(strncmp(i->name->string, "flag", 4u) == 0u) {
            auto flag_string = json_value_as_string(i->value);

            if(!flag_string) {
                std::puts("Invalid server response!");
                return 3;
            }

            std::puts(flag_string->string);
            return 0;
        }
    }

    std::puts("Flag was not found in server response!");

    return 1;
}
