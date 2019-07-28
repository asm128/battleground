#include "gpk_cgi_app_impl_v2.h"

#include "gpk_process.h"
#include "gpk_chrono.h"
#include "gpk_http_client.h"
#include "gpk_json_expression.h"
#include "gpk_find.h"
#include "gpk_parse.h"

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
	gpk_necall(output.append(::gpk::view_const_string{"Content-type: text/html\r\n"})	, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\r\n"})							, "%s", "Out of memory?");
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
		::gpk::SCoord2<uint32_t>				boardMetrics				= {10, 10};
		uint32_t								mines						= 10;
		::gpk::view_const_string				idGame						= {};
		::gpk::find("game_id"	, requestReceived.QueryStringKeyVals, idGame);
		::gpk::SJSONFile						config						= {};
		const char								configFileName	[]			= "./minefront.json";
		gpk_necall(::gpk::jsonFileRead(config, configFileName), "Failed to load configuration file: %s.", configFileName);
		{
			::gpk::view_const_string			backendIpString				= {};
			::gpk::view_const_string			backendPortString			= {};
			gpk_necall(::gpk::jsonExpressionResolve("mineback.http_address"	, config.Reader, 0, backendIpString), "Backend address not found in config file: %s.", configFileName);
			gpk_necall(::gpk::jsonExpressionResolve("mineback.http_port"	, config.Reader, 0, backendPortString), "Backend address not found in config file: %s.", configFileName);
			::gpk::tcpipInitialize();
			::gpk::SIPv4						backendAddress				= {};
			gpk_necall(::gpk::tcpipAddress(backendIpString, {}, backendAddress), "%s", "Cannot resolve host.");
			::gpk::array_pod<char_t>			backendResponse				= {};
			if(idGame.size()) {
				::gpk::view_const_string				actionName				= {};
				::gpk::view_const_string				actionX					= {};
				::gpk::view_const_string				actionY					= {};
				::gpk::find("x"			, requestReceived.QueryStringKeyVals, actionX		);
				::gpk::find("y"			, requestReceived.QueryStringKeyVals, actionY		);
				::gpk::find("name"		, requestReceived.QueryStringKeyVals, actionName	);
				::gpk::array_pod<char>					temp					= ::gpk::view_const_string{"/mineback.exe/action?x="};
				temp.append(actionX);
				temp.append(::gpk::view_const_string{"&y="});
				temp.append(actionY);
				temp.append(::gpk::view_const_string{"&name="});
				temp.append(actionName);
				temp.append(::gpk::view_const_string{"&game_id="});
				temp.append(idGame);
				gpk_necall(::gpk::httpClientRequest(backendAddress, ::gpk::HTTP_METHOD_GET, "asm128.com", {temp.begin(), temp.size()}, "", "", backendResponse), "Cannot connect to backend service.");
			}
			else {
				::gpk::view_const_string				boardWidth					= {};
				::gpk::view_const_string				boardHeight					= {};
				::gpk::view_const_string				boardMines					= {};
				::gpk::find("width"		, requestReceived.QueryStringKeyVals, boardWidth	);
				::gpk::find("height"	, requestReceived.QueryStringKeyVals, boardHeight	);
				::gpk::find("mines"		, requestReceived.QueryStringKeyVals, boardMines	);
				if(boardWidth	.size()) ::gpk::parseIntegerDecimal(boardWidth , &boardMetrics.x);
				if(boardHeight	.size()) ::gpk::parseIntegerDecimal(boardHeight, &boardMetrics.y);
				if(boardMines	.size()) ::gpk::parseIntegerDecimal(boardMines, &mines);
				char								temp[1027]					= {};
				sprintf_s(temp, "/mineback.exe/start?width=%u&height=%u&mines=%u", boardMetrics.x, boardMetrics.y, mines);
				gpk_necall(::gpk::httpClientRequest(backendAddress, ::gpk::HTTP_METHOD_GET, "asm128.com", temp, "", "", backendResponse), "Cannot connect to backend service.");
			}
			::gpk::SJSONReader					jsonResponse;
			if errored(::gpk::jsonParse(jsonResponse, {backendResponse.begin(), backendResponse.size()})) {
				output							= "Content-type: text/html\r\n\r\n <html><body>Failed to parse JSON response from backend service.</body></html>";
				return -1;
			}
			::gpk::view_const_string			strBoard					= {};
			::gpk::error_t						boardNode					= ::gpk::jsonExpressionResolve("board", jsonResponse, 0, strBoard);
			gpk_necall(boardNode, "%s", "time_elapsed not found in backend response.");

			const int32_t						boardHeight					= ::gpk::jsonArraySize(*jsonResponse[boardNode]);
			const int32_t						boardWidth					= ::gpk::jsonArraySize(*jsonResponse[::gpk::jsonArrayValueGet(*jsonResponse[boardNode], 0)]);

			::gpk::view_const_string			strTimeElapsed				= {};
			gpk_necall(::gpk::jsonExpressionResolve("time_elapsed", jsonResponse, 0, strTimeElapsed), "%s", "time_elapsed not found in backend response.");

			gpk_necall(output.append(::gpk::view_const_string{" <tr>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>Elapsed time"})	, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(strTimeElapsed)								, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </tr>"})			, "%s", "Out of memory?");

			//uint32_t							timeElapsed					= 0;
			//::gpk::parseIntegerDecimal(strTimeElapsed, &timeElapsed);


			gpk_necall(output.append(::gpk::view_const_string{" <tr>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td colspan=4>"})	, "%s", "Out of memory?");

			// -- Build the board
			gpk_necall(output.append(::gpk::view_const_string{" <table style=\"border-style:solid;border-color:blue;border-width:1px;\">"}), "%s", "Out of memory?");
			for(int32_t iRow = 0; iRow < boardHeight; ++iRow) {
				gpk_necall(output.append(::gpk::view_const_string{" <tr style=\"border-style:solid;border-color:black;border-width:1px;\">"}), "%s", "Out of memory?");
				const int32_t						rowNode						= ::gpk::jsonArrayValueGet(*jsonResponse[boardNode], iRow);
				for(int32_t iColumn = 0; iColumn < boardWidth; ++iColumn) {
					gpk_necall(output.append(::gpk::view_const_string{" <td style=\"border-style:solid;border-color:black;border-width:1px; width:16px; height:16px; text-align:center\">"}), "%s", "Out of memory?");
					const int32_t						cellNode					= ::gpk::jsonArrayValueGet(*jsonResponse[rowNode], iColumn);
					gpk_necall(output.append(::gpk::view_const_string{" <input type=\"submit\" style=\"border-style:solid;border-color:black;border-width:1px; width:16px; height:16px; text-align:center\" value=\""}), "%s", "Out of memory?");
					gpk_necall(output.append(jsonResponse.View[cellNode]), "%s", "Out of memory?");
					gpk_necall(output.append(::gpk::view_const_string{"\" />"}), "%s", "Out of memory?");
					//gpk_necall(output.append(jsonResponse.View[cellNode]), "%s", "Out of memory?");

					gpk_necall(output.append(::gpk::view_const_string{" </td>"})	, "%s", "Out of memory?");
				}
				gpk_necall(output.append(::gpk::view_const_string{" </tr>"})	, "%s", "Out of memory?");
			}
			gpk_necall(output.append(::gpk::view_const_string{" </table>"})	, "%s", "Out of memory?");


			output.append(::gpk::view_const_string{"<code style=\"white-space: pre-wrap;\">"});
			output.append(::gpk::view_const_string{backendResponse.begin(), (uint32_t)-1});
			output.append(::gpk::view_const_string{"</code>"});


			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </tr>"})			, "%s", "Out of memory?");
			::gpk::tcpipShutdown();
		}
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
