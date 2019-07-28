#include "gpk_cgi_app_impl_v2.h"

#include "gpk_process.h"
#include "gpk_chrono.h"
#include "gpk_http_client.h"
#include "gpk_json_expression.h"

static	const ::gpk::view_const_string			STR_RESPONSE_METHOD_INVALID		= "{ \"status\" : 403, \"description\" :\"Forbidden - Available method/command combinations are: \n- GET / Start, \n- POST / Continue, \n- POST / Step, \n- POST / Flag, \n- POST / Wipe, \n- POST / Hold.\" }\r\n";

GPK_CGI_JSON_APP_IMPL();

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::srand((uint32_t)::gpk::timeCurrentInUs());

	::gpk::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::gpk::httpRequestInit(requestReceived, runtimeValues, true);
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	if (isCGIEnviron) {
		const ::gpk::view_const_string						allowedMethods	[]				= {"GET", "POST"};
		gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
		if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", allowedMethods)) {
			gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\n"}), "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
			gpk_necall(output.append(STR_RESPONSE_METHOD_INVALID), "%s", "Out of memory?");
			return 1;
		}
	}
	gpk_necall(output.append(::gpk::view_const_string{"Content-type: text/html\r\n"}), "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\r\n"})						, "%s", "Out of memory?");
	static constexpr const char							page_title	[]					= "MineFront";
	gpk_necall(output.append(::gpk::view_const_string{"<html>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <head>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <title>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{page_title})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</title>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</head>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <body>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <table>"})						, "%s", "Out of memory?");
	if(0 == requestReceived.QueryString.size()) { // Just print the main form.
		gpk_necall(output.append(::gpk::view_const_string{" <tr>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{" <td>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<form method=\"GET\" action=\"./minefront.exe\">"})	, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>width	</td><td><input type=\"number\" name=\"width\"  min=10 max=256 value=10 /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>height	</td><td><input type=\"number\" name=\"height\" min=10 max=256 value=10 /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>mines	</td><td><input type=\"number\" name=\"mines\"  min=10 max=256 value=10 /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td colspan=2><input type=\"submit\" text=\"Start game!\"/></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"</form>"})					, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</td>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</tr>"})						, "%s", "Out of memory?");
	}
	else {
		::gpk::SJSONFile				config						= {};
		const char						configFileName	[]			= "minefront.json";
		gpk_necall(::gpk::jsonFileRead(config, configFileName), "Failed to load configuraiton file: %s.", configFileName);

		::gpk::view_const_string		backendIpString				= {};
		gpk_necall(::gpk::jsonExpressionResolve("mineback.address", config.Reader, 0, backendIpString), "Backend address not found in config file: %s.", configFileName);
		gpk_necall(::gpk::jsonExpressionResolve("mineback.address", config.Reader, 0, backendIpString), "Backend address not found in config file: %s.", configFileName);
		::gpk::tcpipInitialize();
		::gpk::SIPv4					backendAddress				= {};
		gpk_necall(::gpk::tcpipAddress(backendIpString, {}, backendAddress), "%s", "Cannot resolve host.");
		::gpk::array_pod<char_t>		backendResponse				= {};
		gpk_necall(::gpk::httpClientRequest(backendAddress, ::gpk::HTTP_METHOD_GET, "asm128.com", "/start", "application/json", "", backendResponse), "Cannot connect to backend service.");
		::gpk::tcpipShutdown();
	}
	gpk_necall(output.append(::gpk::view_const_string{"</table>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</body>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</html>"})						, "%s", "Out of memory?");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
