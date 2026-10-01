#!/usr/bin/env python3
"""Read the latest recorded token usage for this homework's Codex chat."""
import json
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo

THREAD_ID = '01a0f150-0f34-7b90-9f00-0b980b9a6614'
latest = None
for path in (Path.home() / '.codex' / 'sessions').rglob(f'*{THREAD_ID}*.jsonl'):
    with path.open(encoding='utf-8') as stream:
        for line in stream:
            try:
                row = json.loads(line)
            except ValueError:
                continue
            payload = row.get('payload', {})
            info = payload.get('info')
            if payload.get('type') != 'token_count' or not info:
                continue
            if latest is None or row['timestamp'] > latest['timestamp']:
                latest = {'timestamp': row['timestamp'], 'info': info}
if latest is None:
    raise SystemExit('No recorded token usage found for this chat.')
usage = latest['info']['total_token_usage']
when = datetime.fromisoformat(latest['timestamp'].replace('Z', '+00:00')).astimezone(ZoneInfo('Asia/Taipei'))
print('Recorded at:', when.strftime('%Y-%m-%d %H:%M:%S UTC+08:00'))
for key in ['input_tokens', 'cached_input_tokens', 'output_tokens', 'reasoning_output_tokens', 'total_tokens']:
    print(f'{key}: {usage.get(key, 0):,}')
print('Cached input is included in input. Reasoning output is included in output.')
print('Read the latest cumulative total; do not sum cumulative records.')
