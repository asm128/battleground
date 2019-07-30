#include "gpk_cgi_app_impl_v2.h"

#include "gpk_process.h"
#include "gpk_chrono.h"
#include "gpk_http_client.h"
#include "gpk_json_expression.h"
#include "gpk_find.h"
#include "gpk_parse.h"
#include "gpk_base64.h"
#include "gpk_udp_client.h"

static	const ::gpk::view_const_string			STR_RESPONSE_METHOD_INVALID		= "{ \"status\" : 403, \"description\" :\"Forbidden - Available method/command combinations are: \n- GET / Start, \n- GET/POST / Continue, \n- GET/POST / Step, \n- GET/POST / Flag, \n- GET/POST / Wipe, \n- GET/POST / Hold.\" }\r\n";

GPK_CGI_JSON_APP_IMPL();

static const ::gpk::view_const_string			javaScriptCode					= {
"\n	function cellClick(idCell) {"
"\n	    var cell = document.getElementById(idCell);"
"\n	    if(cell.value == '')"
"\n			cell.style.backgroundColor = 'orange';"
"\n	    else if(cell.value == '!' || cell.value == '?')"
"\n			cell.style.backgroundColor = 'orange';"
"\n	    else"
"\n			cell.style.backgroundColor = 'white';"
"\n	}"
"\n	function cellOut(idCell) {"
"\n	    var cell = document.getElementById(idCell);"
"\n	    if(cell.value == '') "
"\n			cell.style.backgroundColor = 'lightgray';"
"\n	    else if(cell.value == '!' || cell.value == '?') "
"\n			cell.style.backgroundColor = 'lightgray';"
"\n	    else"
"\n			cell.style.backgroundColor = 'white';"
"\n	}"
"\n	function cellOver(idCell) {"
"\n	    var cell = document.getElementById(idCell);"
"\n	    cell.style.backgroundColor = 'yellow';"
"\n	}"
"\n	function easy() {"
"\n	    var mfront_height = document.getElementById('mfront_height');"
"\n	    var mfront_width  = document.getElementById('mfront_width');"
"\n	    var mfront_mines  = document.getElementById('mfront_mines');"
"\n	    mfront_height .value = '9';"
"\n	    mfront_width  .value = '9';"
"\n	    mfront_mines  .value = '10';"
"\n	}"
"\n	function medium(idCell) {"
"\n	    var mfront_height = document.getElementById('mfront_height');"
"\n	    var mfront_width  = document.getElementById('mfront_width');"
"\n	    var mfront_mines  = document.getElementById('mfront_mines');"
"\n	    mfront_height .value = '16';"
"\n	    mfront_width  .value = '16';"
"\n	    mfront_mines  .value = '40';"
"\n	}"
"\n	function hard(idCell) {"
"\n	    var mfront_height = document.getElementById('mfront_height');"
"\n	    var mfront_width  = document.getElementById('mfront_width');"
"\n	    var mfront_mines  = document.getElementById('mfront_mines');"
"\n	    mfront_height .value = '24';"
"\n	    mfront_width  .value = '24';"
"\n	    mfront_mines  .value = '99';"
"\n	}"
"\n	function cellFlag(idCell) {"
"\n	    var cell	= document.getElementById(idCell);"
"\n	    var actn	= document.getElementById(idCell + '_name');"
"\n	    if(cell.value == '!')"
"\n			actn.value	= 'hold';"
"\n	    else if(cell.value == '?')"
"\n			actn.value	= 'hold';"
"\n		else"
"\n			actn.value	= 'flag';"
"\n	}"
};


static	int											cgiRelay			(const ::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::SIPv4 & backendAddress, ::gpk::array_pod<char> & output)										{
	::gpk::array_pod<char_t>								environmentBlock		= {};
	{	// Prepare CGI environment and request content packet to send to the service.
		ree_if(errored(::gpk::environmentBlockFromEnviron(environmentBlock)), "%s", "Failed");
		environmentBlock.append(runtimeValues.Content.Body.begin(), runtimeValues.Content.Body.size());
		environmentBlock.push_back(0);
	}
	{	// Connect the client to the service.
		::gpk::SUDPClient										bestClient				= {};
		{	// setup and connect
			bestClient.AddressConnect							= backendAddress;
			gpk_necall(::gpk::clientConnect(bestClient), "%s", "error");
		}
		::gpk::array_pod<char_t>								responseRemote;
		{	// Send the request data to the connected service.
			ree_if(bestClient.State != ::gpk::UDP_CONNECTION_STATE_IDLE, "%s", "Failed to connect to server.");
			gpk_necall(::gpk::connectionPushData(bestClient, bestClient.Queue, environmentBlock, true, true), "%s", "error");	// Enqueue the packet
			while(bestClient.State != ::gpk::UDP_CONNECTION_STATE_DISCONNECTED) {	// Loop until we ge the response or the client disconnects
				gpk_necall(::gpk::clientUpdate(bestClient), "%s", "error");
				::gpk::array_obj<::gpk::ptr_obj<::gpk::SUDPConnectionMessage>>	received;
				{	// pick up messages for later processing
					::gpk::mutex_guard												lockRecv					(bestClient.Queue.MutexReceive);
					received													= bestClient.Queue.Received;
					bestClient.Queue.Received.clear();
				}
				if(received.size()) {	// Response has been received. Break loop.
					responseRemote												= received[0]->Payload;
					break;
				}
			}
		}
		//info_printf("Remote CGI answer: %s.", responseRemote.begin());
		gpk_necall(::gpk::clientDisconnect(bestClient), "%s", "error");
		output									= responseRemote;
	}
	return 0;
}

::gpk::error_t									gpk_cgi_generate_output			(::gpk::SCGIRuntimeValues & runtimeValues, ::gpk::array_pod<char_t> & output)	{
	::srand((uint32_t)::gpk::timeCurrentInUs());

	::gpk::SHTTPAPIRequest								requestReceived					= {};
	bool												isCGIEnviron					= ::gpk::httpRequestInit(requestReceived, runtimeValues, true);
	::gpk::array_obj<::gpk::TKeyValConstString>			environViews;
	if (isCGIEnviron) {
		const ::gpk::view_const_string						allowedMethods	[]				= {"GET", "POST", "get", "post"};
		gpk_necall(::gpk::environmentBlockViews(runtimeValues.EntryPointArgs.EnvironmentBlock, environViews), "%s", "If this breaks, we better know ASAP.");
		if(0 == ::gpk::keyValVerify(environViews, "REQUEST_METHOD", allowedMethods)) {
			gpk_necall(output.append(::gpk::view_const_string{"Content-type: application/json\r\nCache-Control: no-cache\r\n"}), "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"\r\n"})								, "%s", "Out of memory?");
			gpk_necall(output.append(STR_RESPONSE_METHOD_INVALID), "%s", "Out of memory?");
			return 1;
		}
	}

	gpk_necall(output.append(::gpk::view_const_string{"Content-type: text/html\r\nCache-Control: no-cache\r\n"})	, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\r\n"})							, "%s", "Out of memory?");
	static constexpr const char							page_title	[]					= "MineFront";
	gpk_necall(output.append(::gpk::view_const_string{"<html>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <head>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <title>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{page_title})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</title>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <script>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{javaScriptCode})					, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</script>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"</head>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <body>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{" <table>"})						, "%s", "Out of memory?");
	if(0 == requestReceived.QueryString.size()) { // Just print the main form.
		gpk_necall(output.append(::gpk::view_const_string{" <tr>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{" <td>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<form method=\"GET\" action=\"/minefront.exe/start\">"})	, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{" <table>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>width	</td><td><input type=\"number\" id=\"mfront_width\" name=\"width\"	 min=4 max=50	value=32 /></td><td><input type=\"button\" value=\"easy\"	onclick=\"easy();	/*submit();*/\" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>height	</td><td><input type=\"number\" id=\"mfront_height\" name=\"height\" min=4 max=50  value=32 /></td><td><input type=\"button\" value=\"medium\"	onclick=\"medium(); /*submit();*/\" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>mines	</td><td><input type=\"number\" id=\"mfront_mines\" name=\"mines\"	 min=4 max=500	value=64 /></td><td><input type=\"button\" value=\"hard\"	onclick=\"hard();	/*submit();*/\" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td colspan=2><input type=\"submit\" value=\"Start game!\"/></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{" </table>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</form>"})					, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</td>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</tr>"})						, "%s", "Out of memory?");
	}
	else {
		::gpk::SCoord2<uint32_t>				boardMetrics				= {10, 10};
		::gpk::view_const_string				idGame						= {};
		::gpk::find("game_id"	, requestReceived.QueryStringKeyVals, idGame);
		::gpk::SJSONFile						config						= {};
		const char								configFileName	[]			= "./minefront.json";
		gpk_necall(::gpk::jsonFileRead(config, configFileName), "Failed to load configuration file: %s.", configFileName);
		{
			::gpk::SJSONReader						jsonResponse;
			::gpk::SIPv4							backendAddress				= {};
			::gpk::view_const_string				udpLanIpString				= {};
			::gpk::view_const_string				udpLanPortString			= {};
			::gpk::error_t							indexNodeUdpLanIp			= ::gpk::jsonExpressionResolve("mineback.udp_lan_address", config.Reader, 0, udpLanIpString	);
			::gpk::error_t							indexNodeUdpLanPort			= ::gpk::jsonExpressionResolve("mineback.udp_lan_port"	, config.Reader, 0, udpLanPortString);
			gwarn_if(errored(indexNodeUdpLanIp		), "Backend address not found in config file: %s.", configFileName);
			gwarn_if(errored(indexNodeUdpLanPort	), "Backend address not found in config file: %s.", configFileName);
			::gpk::tcpipInitialize();
			if(0 <= indexNodeUdpLanIp)
				gpk_necall(::gpk::tcpipAddress(udpLanIpString, udpLanPortString, backendAddress), "%s", "Cannot resolve host.");
			else {
				::gpk::view_const_string				backendIpString				= {};
				::gpk::view_const_string				backendPortString			= {};
				gpk_necall(::gpk::jsonExpressionResolve("mineback.http_address"	, config.Reader, 0, backendIpString), "Backend address not found in config file: %s.", configFileName);
				gpk_necall(::gpk::jsonExpressionResolve("mineback.http_port"	, config.Reader, 0, backendPortString), "Backend address not found in config file: %s.", configFileName);
				gpk_necall(::gpk::tcpipAddress(backendIpString, backendPortString, backendAddress), "%s", "Cannot resolve host.");
			}
			::gpk::array_pod<char_t>				backendResponse				= {};
			::gpk::view_const_string				responseBody				= {};
			if(0 <= indexNodeUdpLanIp) {
				int8_t attempts	= 4;
				while((--attempts) > 0) {
					backendResponse.clear();
					jsonResponse = {};
					::cgiRelay(runtimeValues, backendAddress, backendResponse);
					::gpk::error_t				offsetBody = ::gpk::find_sequence_pod(::gpk::view_const_string{"\r\n\r\n"}, {backendResponse.begin(), backendResponse.size()});
					if(0 > offsetBody) {
						offsetBody				= ::gpk::find_sequence_pod(::gpk::view_const_string{"\r\r\n\r\r\n"}, {backendResponse.begin(), backendResponse.size()});
						if(0 > offsetBody)
							offsetBody				= 0;
						else
							offsetBody				+= 6;
					}
					else
						offsetBody				+= 4;
					responseBody			= {&backendResponse[offsetBody], backendResponse.size() - offsetBody};
					if errored(::gpk::jsonParse(jsonResponse, responseBody)) {
						if(attempts == 1) {
							output							= ::gpk::view_const_string{"Content-type: text/html\r\nCache-Control: no-cache\r\n\r\n <html><body>Failed to parse JSON response from backend service."};
							output.append(::gpk::view_const_string{"<code>"});
							output.append(::gpk::view_const_string{backendResponse.begin(), (uint32_t)-1});
							output.append(::gpk::view_const_string{"</code>"});
							output.append(::gpk::view_const_string{"</body></html>"});
							return -1;
						}
					}
					else
						break;

				}
			} else {
				if(idGame.size()) {
					::gpk::view_const_string				actionName				= {};
					::gpk::view_const_string				actionX					= {};
					::gpk::view_const_string				actionY					= {};
					::gpk::find("x"			, requestReceived.QueryStringKeyVals, actionX		);
					::gpk::find("y"			, requestReceived.QueryStringKeyVals, actionY		);
					::gpk::find("name"		, requestReceived.QueryStringKeyVals, actionName	);
					::gpk::array_pod<char>					temp					= ::gpk::view_const_string{"/mineback.exe/action?x="};
					gpk_necall(temp.append(actionX)									, "%s", "Out of memory?");
					gpk_necall(temp.append(::gpk::view_const_string{"&y="})			, "%s", "Out of memory?");
					gpk_necall(temp.append(actionY)									, "%s", "Out of memory?");
					gpk_necall(temp.append(::gpk::view_const_string{"&name="})		, "%s", "Out of memory?");
					gpk_necall(temp.append(actionName)								, "%s", "Out of memory?");
					gpk_necall(temp.append(::gpk::view_const_string{"&game_id="})	, "%s", "Out of memory?");
					gpk_necall(temp.append(idGame)									, "%s", "Out of memory?");
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
					uint32_t								mines						= 10;
					if(boardMines	.size()) ::gpk::parseIntegerDecimal(boardMines, &mines);
					char									temp[1027]					= {};
					sprintf_s(temp, "/mineback.exe/start?width=%u&height=%u&mines=%u", boardMetrics.x, boardMetrics.y, mines);
					gpk_necall(::gpk::httpClientRequest(backendAddress, ::gpk::HTTP_METHOD_GET, "asm128.com", temp, "", "", backendResponse), "Cannot connect to backend service.");
				}
				responseBody			= {backendResponse.begin(), backendResponse.size()};
				if errored(::gpk::jsonParse(jsonResponse, responseBody)) {
					output							= ::gpk::view_const_string{"Content-type: text/html\r\nCache-Control: no-cache\r\n\r\n <html><body>Failed to parse JSON response from backend service.</body>"};
					output.append(::gpk::view_const_string{"<code>"});
					output.append(::gpk::view_const_string{backendResponse.begin(), (uint32_t)-1});
					output.append(::gpk::view_const_string{"</code>"});
					output.append(::gpk::view_const_string{"</html>"});
					return -1;
				}
			}
			::gpk::tcpipShutdown();

			if(0 == idGame.size())
				gpk_necall(::gpk::jsonExpressionResolve("game_id", jsonResponse, 0, idGame), "%s", "time_elapsed not found in backend response.");

			::gpk::view_const_string			strBoard					= {};
			::gpk::error_t						boardNode					= ::gpk::jsonExpressionResolve("board", jsonResponse, 0, strBoard);
			gpk_necall(boardNode, "%s", "time_elapsed not found in backend response.");

			const int32_t						boardHeight					= ::gpk::jsonArraySize(*jsonResponse[boardNode]);
			const int32_t						boardWidth					= ::gpk::jsonArraySize(*jsonResponse[::gpk::jsonArrayValueGet(*jsonResponse[boardNode], 0)]);

			::gpk::view_const_string			strTimeElapsed				= {};
			gpk_necall(::gpk::jsonExpressionResolve("time_elapsed", jsonResponse, 0, strTimeElapsed), "%s", "time_elapsed not found in backend response.");
			::gpk::view_const_string			strWon						= {};
			gpk_necall(::gpk::jsonExpressionResolve("won", jsonResponse, 0, strWon), "%s", "time_elapsed not found in backend response.");

			gpk_necall(output.append(::gpk::view_const_string{" <tr>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>Elapsed time"})	, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(strTimeElapsed)								, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>Won"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(strWon)										, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" </tr>"})			, "%s", "Out of memory?");

			//uint32_t							timeElapsed					= 0;
			//::gpk::parseIntegerDecimal(strTimeElapsed, &timeElapsed);
			gpk_necall(output.append(::gpk::view_const_string{" <tr>"})				, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td colspan=4 style=\"text-align:center;\">"})	, "%s", "Out of memory?");

			// -- Build the board
			gpk_necall(output.append(::gpk::view_const_string{"\n<table style=\"border-style:inset;border-color:lightgray;border-width:1px;\">"}), "%s", "Out of memory?");
			char								tempCoord	[64]			= {};
			char								tempScript	[8192]			= {};
			::gpk::array_pod<char_t>			strGameId					= idGame;
			strGameId.push_back(0);
			::gpk::array_pod<char_t>			b64Encoded					= {};
			for(int32_t iRow = 0; iRow < boardHeight; ++iRow) {
				gpk_necall(output.append(::gpk::view_const_string{"\n<tr style=\"border-style:none;border-color:black;border-width:0px;\">"}), "%s", "Out of memory?");
				const int32_t						rowNode						= ::gpk::jsonArrayValueGet(*jsonResponse[boardNode], iRow);
				for(int32_t iColumn = 0; iColumn < boardWidth; ++iColumn) {
					gpk_necall(output.append(::gpk::view_const_string{"\n<td style=\"border-style:none;border-color:black;border-width:0px; width:16px; height:16px; text-align:center;padding:0px;\">"}), "%s", "Out of memory?");
					const int32_t						cellNode					= ::gpk::jsonArrayValueGet(*jsonResponse[rowNode], iColumn);
					sprintf_s(tempCoord, "%0.3u%0.3u", iColumn, iRow);
					::gpk::base64EncodeFS(::gpk::view_const_string{tempCoord}, b64Encoded);
					b64Encoded.push_back(0);
					sprintf_s(tempScript, "\n<form style=\"padding:0px; margin:0px; width:16px; height:16px; \" method=\"GET\" action=\"http://201.235.131.233/minefront.exe/action\" >"
						"<input type=\"hidden\" id=\"%s_name\"		name=\"name\"		value=\"step\" >"
						"<input type=\"hidden\" id=\"%s_x\"			name=\"x\"			value=\"%u\" >"
						"<input type=\"hidden\" id=\"%s_x\"			name=\"y\"			value=\"%u\" >"
						"<input type=\"hidden\" id=\"%s_game_id\"	name=\"game_id\"	value=\"%s\" >"
						"<input type=\"submit\" id=\"%s\" oncontextmenu=\"cellFlag('%s');submit(); return false;\"  onclick=\"cellClick('%s')\" onmouseout=\"cellOut('%s')\" onmouseover=\"cellOver('%s')\" "
						, b64Encoded.begin()
						, b64Encoded.begin(), iColumn
						, b64Encoded.begin(), iRow
						, b64Encoded.begin(), strGameId.begin()
						, b64Encoded.begin(), b64Encoded.begin(), b64Encoded.begin(), b64Encoded.begin(), b64Encoded.begin()
						);
					gpk_necall(output.append(::gpk::view_const_string{tempScript}), "%s", "Out of memory?");
					if('~' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"text-color:lightgray;border-style:outset;background-color:lightgray;border-width:1px; width:16px; height:16px; text-align:center;\" value=\""}), "%s", "Out of memory?");
					else if('!' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;text-color:lightgray;border-style:outset;background-color:lightgray;border-width:1px; width:16px; height:16px; text-align:center;color:red;\" value=\""}), "%s", "Out of memory?");
					else if('?' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;text-color:lightgray;border-style:outset;background-color:lightgray;border-width:1px; width:16px; height:16px; text-align:center;color:red;\" value=\""}), "%s", "Out of memory?");
					else if('1' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:blue;\" value=\""}), "%s", "Out of memory?");
					else if('2' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:green;\" value=\""}), "%s", "Out of memory?");
					else if('3' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:purple;\" value=\""}), "%s", "Out of memory?");
					else if('4' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:darkgray;\" value=\""}), "%s", "Out of memory?");
					else if('5' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:violet;\" value=\""}), "%s", "Out of memory?");
					else if('6' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:yellow;\" value=\""}), "%s", "Out of memory?");
					else if('7' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:magenta;\" value=\""}), "%s", "Out of memory?");
					else if('8' == jsonResponse.View[cellNode][0])
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:red;\" value=\""}), "%s", "Out of memory?");
					else
						gpk_necall(output.append(::gpk::view_const_string{"style=\"font-weight:bold;border-style:none;background-color:white;border-width:1px; width:16px; height:16px; text-align:center;color:red;\" value=\""}), "%s", "Out of memory?");
					b64Encoded.clear();
					if('~' != jsonResponse.View[cellNode][0])
						gpk_necall(output.append(jsonResponse.View[cellNode]), "%s", "Out of memory?");
					gpk_necall(output.append(::gpk::view_const_string{"\" /></form>"}), "%s", "Out of memory?");
					gpk_necall(output.append(::gpk::view_const_string{"\n</td>"})	, "%s", "Out of memory?");
					//OutputDebugStringA(output.begin());
					//"/action?name=hold&x=0&y=0&game_id=MjAxLjIzNS4xMzEuMjMzX18xNTY0MzM1ODM5NDkyNDg5"
				}
				gpk_necall(output.append(::gpk::view_const_string{"\n</tr>"})	, "%s", "Out of memory?");
			}
			gpk_necall(output.append(::gpk::view_const_string{"\n</table>"})	, "%s", "Out of memory?");

			//output.append(::gpk::view_const_string{"\n<code style=\"white-space: pre-wrap;\">"});
			//output.append(::gpk::view_const_string{backendResponse.begin(), (uint32_t)-1});
			//output.append(::gpk::view_const_string{"\n</code>"});

			gpk_necall(output.append(::gpk::view_const_string{"\n</td>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"\n</tr>"})			, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <tr>"})						, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{" <td colspan=4>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<form method=\"GET\" action=\"/minefront.exe/start\">"})	, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{" <table>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>width	</td><td><input type=\"number\" id=\"mfront_width\" name=\"width\"	 min=4 max=50  value=32 /></td><td><input type=\"button\" value=\"easy\"	onclick=\"easy();	\" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>height	</td><td><input type=\"number\" id=\"mfront_height\" name=\"height\" min=4 max=50  value=32 /></td><td><input type=\"button\" value=\"medium\"	onclick=\"medium(); \" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td>mines	</td><td><input type=\"number\" id=\"mfront_mines\" name=\"mines\"	 min=4 max=500 value=64 /></td><td><input type=\"button\" value=\"hard\"	onclick=\"hard();	\" /></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{"<tr><td colspan=2><input type=\"submit\" value=\"Start new game!\"/></td></tr>"}), "%s", "Out of memory?");	//
		gpk_necall(output.append(::gpk::view_const_string{" </table>"})						, "%s", "Out of memory?");
		gpk_necall(output.append(::gpk::view_const_string{"</form>"})					, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"</td>"})						, "%s", "Out of memory?");
			gpk_necall(output.append(::gpk::view_const_string{"</tr>"})						, "%s", "Out of memory?");

		}
	}
	gpk_necall(output.append(::gpk::view_const_string{"\n</table>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n</body>"})						, "%s", "Out of memory?");
	gpk_necall(output.append(::gpk::view_const_string{"\n</html>"})						, "%s", "Out of memory?");
	if(output.size()) {
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
	}
	return 0;
}
