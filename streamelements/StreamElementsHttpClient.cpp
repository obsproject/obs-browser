#include "StreamElementsHttpClient.hpp"
#include "StreamElementsUtils.hpp"

#include <codecvt>
#include <string>

void StreamElementsHttpClient::DeserializeHttpRequestText(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> output)
{
	bool success = false;
	std::wstring result_string = L"";
	std::string error_string = "";
	int http_status_code = 0;

	if (input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		std::string method =
			d->HasKey("method") ? d->GetString("method").ToString() : "GET";

		std::string url =
			d->HasKey("url") ? d->GetString("url") : "";

		http_client_request_headers_t request_headers;

		if (d->HasKey("headers") && d->GetType("headers") == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> h = d->GetDictionary("headers");
			CefDictionaryValue::KeyList keys;

			if (h->GetKeys(keys)) {
				for (std::string key : keys) {
					if (h->GetType(key) == VTYPE_STRING) {
						request_headers.emplace(
							std::make_pair<std::string, std::string>(
								key.c_str(),
								h->GetString(key).ToString()));
					}
					else if (h->GetType(key) == VTYPE_LIST) {
						CefRefPtr<CefListValue> l = h->GetList(key);

						for (size_t i = 0; i < l->GetSize(); ++i) {
							if (l->GetType(i) != VTYPE_STRING) {
								continue;
							}

							std::string val = l->GetString(i);

							request_headers.emplace(
								std::make_pair<std::string, std::string>(
									key.c_str(),
									val.c_str()));
						}
					}
				}
			}
		}

		std::transform(method.begin(), method.end(), method.begin(), ::toupper);

		if (url.size() && method.size()) {

		}

		std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

		auto cb = [&](char* data, void* userdata, char* error_msg, int http_code)
		{
			if (http_code != 0) {
				http_status_code = http_code;
			}

			if (error_msg) {
				error_string = error_msg;
			}

			if (data) {
				try {
					result_string = myconv.from_bytes(data);
				}
				catch(...) {
					result_string = L"Invalid UTF-8 input";
					error_string = "Failed decoding UTF-8 response data";
				}
			}
		};

		if (method == "GET") {
			success = HttpGetString(
				url.c_str(),
				request_headers,
				cb,
				nullptr);
		}
		else if (method == "POST") {
			std::string post_data;

			if (d->HasKey("body") && d->GetType("body") == VTYPE_STRING) {
				post_data = myconv.to_bytes(
					d->GetString("body").ToWString());
			}

			success = HttpPostString(
				url.c_str(),
				request_headers,
				post_data.c_str(),
				cb,
				nullptr);
		}
	}

	CefRefPtr<CefDictionaryValue> output_dict = CefDictionaryValue::Create();

	output_dict->SetBool("success", success);

	if (success) {
		output_dict->SetString("body", result_string);
	}
	else {
		output_dict->SetNull("body");
	}

	output_dict->SetInt("statusCode", http_status_code);

	if (error_string.size()) {
		output_dict->SetString("statusText", error_string.c_str());
	}
	else {
		output_dict->SetString("statusText", "OK");
	}

	output->SetDictionary(output_dict);
}
