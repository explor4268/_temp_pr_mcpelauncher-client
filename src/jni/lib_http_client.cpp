#include "lib_http_client.h"
#include "../util.h"
#include <log.h>
#include <curl/curl.h>

using namespace std::placeholders;

Ecdsa::Ecdsa() = default;

EC_KEY *Ecdsa::eckey = NULL;
EC_GROUP *Ecdsa::ecgroup = NULL;
std::shared_ptr<FakeJni::JString> Ecdsa::unique_id = NULL;

HttpClientRequest::HttpClientRequest() {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpClientRequest::write_callback_wrapper);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HttpClientRequest::header_callback_wrapper);
}

HttpClientRequest::~HttpClientRequest() {
    curl_easy_cleanup(curl);
}

FakeJni::JBoolean HttpClientRequest::isNetworkAvailable(std::shared_ptr<Context> context) {
    return true;
}

std::shared_ptr<HttpClientRequest> HttpClientRequest::createClientRequest() {
    return std::make_shared<HttpClientRequest>();
}

void HttpClientRequest::setHttpUrl(std::shared_ptr<FakeJni::JString> url) {
    auto url2 = url->asStdString();
    curl_easy_setopt(curl, CURLOPT_URL, url2.c_str());
#ifdef XAL_HTTP_LOG
    Log::trace("xal-http", "setHttpUrl: '%s'", url2.c_str());
#endif
}

void HttpClientRequest::setHttpMethodAndBody(std::shared_ptr<FakeJni::JString> method,
                                             std::shared_ptr<FakeJni::JString> contentType,
                                             std::shared_ptr<FakeJni::JByteArray> body) {
    this->method = method->asStdString();
#ifdef XAL_HTTP_LOG
    Log::trace("xal-http", "setHttpMethod: '%s'", this->method.c_str());
#endif
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, this->method.c_str());
    this->body = body ? std::vector<char>((char*)body->getArray(), (char*)body->getArray() + body->getSize()) : std::vector<char>{};
    if(this->body.size()) {
#ifdef XAL_HTTP_LOG
        Log::trace("xal-http", "setHttpBody: '%s'", this->body.c_str());
#endif
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, this->body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, this->body.size());
    }
    auto conttype = contentType->asStdString();
    if(conttype.length()) {
        header = curl_slist_append(header, ("Content-Type: " + conttype).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
#ifdef XAL_HTTP_LOG
        Log::trace("xal-http", "setHttpContentType: '%s'", conttype.c_str());
#endif
    }
}

void HttpClientRequest::setHttpHeader(std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> value) {
    header = curl_slist_append(header, (name->asStdString() + ": " + value->asStdString()).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
#ifdef XAL_HTTP_LOG
    Log::trace("xal-http", "setHttpHeader: '%s: %s'", name->asStdString().c_str(), value->asStdString().c_str());
#endif
}

void HttpClientRequest::doRequestAsync(FakeJni::JLong sourceCall) {
    auto ret = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    FakeJni::LocalFrame frame;
    if (ret == CURLE_OK) {
        auto method = getClass().getMethod("(JLcom/xbox/httpclient/HttpClientResponse;)V", "OnRequestCompleted");
        method->invoke(frame.getJniEnv(), this, sourceCall, frame.getJniEnv().createLocalReference(std::make_shared<HttpClientResponse>(response_code, response, headers)));
#ifdef XAL_HTTP_LOG
        Log::trace("xal-http", "doRequestAsync: '%ld %s'", response_code, response.c_str());
#endif
    }
    else {
        auto method = getClass().getMethod("(JLjava/lang/String;)V", "OnRequestFailed");
        method->invoke(frame.getJniEnv(), this, sourceCall, frame.getJniEnv().createLocalReference(std::make_shared<FakeJni::JString>("Error")));
    }
}

FakeJni::JInt HttpClientResponse::getNumHeaders() {
    return headers.size();
}

std::shared_ptr<FakeJni::JString> HttpClientResponse::getHeaderNameAtIndex(FakeJni::JInt index) {
    return std::make_shared<FakeJni::JString>(headers[index].name);
}

std::shared_ptr<FakeJni::JString> HttpClientResponse::getHeaderValueAtIndex(FakeJni::JInt index) {
    return std::make_shared<FakeJni::JString>(headers[index].value);
}

std::shared_ptr<FakeJni::JByteArray> HttpClientResponse::getResponseBodyBytes() {
    return std::make_shared<FakeJni::JByteArray>(body);
}

FakeJni::JInt HttpClientResponse::getResponseCode() {
    return response_code;
}

HttpClientResponse::HttpClientResponse(int response_code, std::vector<signed char> body, std::vector<ResponseHeader> headers) :
    response_code(response_code),
    body(body),
    headers(headers)
{}

size_t HttpClientRequest::write_callback(char *ptr, size_t size, size_t nmemb) {
    response.insert(response.end(), ptr, ptr + nmemb);
    return size * nmemb;
}

size_t HttpClientRequest::header_callback(char *buffer, size_t size, size_t nitems) {
    auto string = std::string(buffer, nitems);
    auto location = string.find(": ");
    if (location != std::string::npos) {
        auto name = string.substr(0, location);
        auto value = string.substr(location+1, string.length());
        trim(name);
        trim(value);
        headers.emplace_back(name, value);
    }
    return size * nitems;
}
