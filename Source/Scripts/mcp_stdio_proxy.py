#!/usr/bin/env python3
"""Stdio MCP proxy that forwards tool calls to WeilanEngine's HTTP MCP server."""

import argparse
import asyncio
import json
import logging
import sys
import uuid

import httpx
import mcp.types as types
from mcp.server import Server
from mcp.server.stdio import stdio_server

logger = logging.getLogger("weilan-mcp-proxy")

ENGINE_UNREACHABLE_MSG = (
    "WeilanEngine is not running or MCP is not enabled. "
    "Start the engine with --enable-mcp."
)

TOOLS = [
    types.Tool(
        name="GetSceneGraph",
        description="Gets the names of all root game objects in the active scene",
        inputSchema={"type": "object", "properties": {}},
    ),
    types.Tool(
        name="AddPrimitiveAssetToScene",
        description="Adds a primitive/model asset to the active scene",
        inputSchema={
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Asset path, for example _engine_internal/Models/Cube.fbx",
                },
            },
            "required": ["path"],
        },
    ),
]


def build_jsonrpc_request(method: str, params: dict | None = None) -> dict:
    req = {
        "jsonrpc": "2.0",
        "id": str(uuid.uuid4()),
        "method": method,
    }
    if params is not None:
        req["params"] = params
    return req


async def forward_tool_call(
    client: httpx.AsyncClient,
    engine_url: str,
    tool_name: str,
    arguments: dict | None,
) -> list[types.TextContent]:
    params: dict = {"name": tool_name}
    if arguments:
        params["arguments"] = arguments

    payload = build_jsonrpc_request("tools/call", params)
    message_url = f"{engine_url.rstrip('/')}/message"

    try:
        resp = await client.post(
            message_url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=30.0,
        )
        resp.raise_for_status()
    except (httpx.ConnectError, httpx.TimeoutException, OSError) as exc:
        logger.error("Engine unreachable: %s", exc)
        return [types.TextContent(type="text", text=ENGINE_UNREACHABLE_MSG)]
    except httpx.HTTPStatusError as exc:
        logger.error("Engine HTTP error: %s", exc)
        return [types.TextContent(type="text", text=f"Engine returned HTTP {exc.response.status_code}")]

    try:
        body = resp.json()
    except (json.JSONDecodeError, ValueError) as exc:
        logger.error("Invalid JSON from engine: %s", exc)
        return [types.TextContent(type="text", text="Engine returned invalid JSON")]

    if "error" in body:
        err = body["error"]
        msg = err.get("message", "Unknown engine error")
        logger.error("Engine JSON-RPC error: %s", err)
        return [types.TextContent(type="text", text=f"Engine error: {msg}")]

    result = body.get("result", {})
    content_list = result.get("content", [])
    items: list[types.TextContent] = []
    for item in content_list:
        items.append(types.TextContent(type="text", text=item.get("text", "")))
    return items if items else [types.TextContent(type="text", text="(empty response)")]


def create_server(engine_url: str) -> Server:
    server = Server("weilan-engine-mcp")
    client = httpx.AsyncClient()

    @server.list_tools()
    async def list_tools() -> list[types.Tool]:
        return TOOLS

    @server.call_tool()
    async def call_tool(name: str, arguments: dict | None) -> list[types.TextContent]:
        logger.info("Tool call: %s(%s)", name, arguments)
        known_names = {t.name for t in TOOLS}
        if name not in known_names:
            return [types.TextContent(type="text", text=f"Unknown tool: {name}")]
        return await forward_tool_call(client, engine_url, name, arguments)

    return server


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="WeilanEngine MCP stdio proxy")
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Engine MCP server port (default: 8080)",
    )
    parser.add_argument(
        "--engine-url",
        type=str,
        default=None,
        help="Full engine URL (default: http://localhost:<port>)",
    )
    return parser.parse_args()


async def main() -> None:
    args = parse_args()
    engine_url = args.engine_url or f"http://localhost:{args.port}"

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
        stream=sys.stderr,
    )
    logger.info("Starting proxy → %s", engine_url)

    server = create_server(engine_url)

    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, server.create_initialization_options())


if __name__ == "__main__":
    asyncio.run(main())
