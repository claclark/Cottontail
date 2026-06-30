#!/usr/bin/env python3

import json
import socket
import sys

try:
    import readline
except ImportError:
    readline = None


def usage(program_name):
    print(f"usage: {program_name} port", file=sys.stderr)


def connect_local(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(("127.0.0.1", port))
    except OSError:
        sock.close()
        raise
    return sock


def request(stream, query):
    line = json.dumps(query, separators=(",", ":")) + "\n"
    stream.write(line.encode("utf-8"))
    stream.flush()
    response = stream.readline()
    if not response:
        return None
    return json.loads(response.decode("utf-8"))


def print_record(record, qid, program_name):
    response = record.get("response", {})
    op = response.get("op", "")
    if not response.get("ok", False):
        print(
            f"{program_name}: {response.get('error', 'unknown error')}",
            file=sys.stderr,
        )
        return qid
    if "time" in record and op == "query":
        print(f"Ranking took: {record['time']} millisecond(s)", file=sys.stderr)
    if response.get("done", False):
        if op == "query" and "qid" in response:
            return response["qid"]
        return qid
    if "qid" in response:
        qid = response["qid"]
    if "snippet" in response:
        print(response["snippet"], flush=True)
    elif "document" in response:
        print(response["document"], flush=True)
    return qid


def main(argv):
    program_name = argv[0]
    if len(argv) != 2 or argv[1] == "--help":
        usage(program_name)
        return 0 if len(argv) == 2 else 1
    try:
        port = int(argv[1])
    except ValueError:
        print(f"{program_name}: bad port: {argv[1]}", file=sys.stderr)
        return 1
    try:
        sock = connect_local(port)
    except OSError:
        print(f"{program_name}: cannot connect to 127.0.0.1:{port}",
              file=sys.stderr)
        return 1
    qid = ""
    with sock, sock.makefile("rwb") as stream:
        while True:
            try:
                line = input(">> ")
            except EOFError:
                break
            if line == "":
                if qid == "":
                    continue
                query = {"op": "next", "qid": qid}
            else:
                if readline is not None:
                    readline.add_history(line)
                query = {"op": "query", "query": line}
            try:
                record = request(stream, query)
            except (OSError, json.JSONDecodeError) as e:
                print(f"{program_name}: bad server response: {e}",
                      file=sys.stderr)
                return 1
            if record is None:
                print(f"{program_name}: server connection closed",
                      file=sys.stderr)
                return 1
            qid = print_record(record, qid, program_name)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
