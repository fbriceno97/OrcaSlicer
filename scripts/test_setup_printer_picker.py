#!/usr/bin/env python3
"""Exercise the real setup printer page with JSON profiles and a mocked bridge.

This is an automated Chromium check, not interactive native Windows verification.
"""
import argparse
import functools
import http.server
from pathlib import Path
import shutil
import tempfile
import threading
from urllib.parse import urlsplit

from playwright.sync_api import expect, sync_playwright
from check_setup_profiles import check_profiles
from test_homepage_login import QuietHandler


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profiles', type=Path, default=root / 'resources/profiles')
    parser.add_argument('--screenshot', type=Path)
    args = parser.parse_args()
    _, models = check_profiles(args.profiles)
    # Reproduce the old shipped layout: caches/covers without the JSON manifests.
    # It must fail before a candidate can be uploaded, even if a cache is present.
    with tempfile.TemporaryDirectory() as empty:
        (Path(empty) / 'BBL.opc').write_bytes(b'cache-only fixture')
        try:
            check_profiles(empty)
        except ValueError as error:
            assert 'Bambu Lab A1' in str(error)
        else:
            raise AssertionError('Cache-only packaging unexpectedly passed')

    browser_path = next((shutil.which(name) for name in
                         ('chromium', 'chromium-browser', 'google-chrome')
                         if shutil.which(name)), None)
    if not browser_path:
        raise SystemExit('An installed Chromium/Chrome browser is required')
    server = http.server.ThreadingHTTPServer(
        ('127.0.0.1', 0), functools.partial(QuietHandler, directory=str(root / 'resources/web')))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    origin = f'http://127.0.0.1:{server.server_port}'
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(executable_path=browser_path)
            context = browser.new_context(user_agent='BBL-Slicer setup-regression',
                                          color_scheme='dark', viewport={'width': 1100, 'height': 850})
            context.route('**/*', lambda route: route.continue_()
                          if urlsplit(route.request.url).netloc == urlsplit(origin).netloc
                          else route.abort())
            context.add_init_script('''
                window.__commands = [];
                window.wx = {postMessage: value => window.__commands.push(JSON.parse(value))};
            ''')
            page = context.new_page()
            errors = []
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(origin + '/guide/21/index.html')
            page.wait_for_function("window.__commands.some(v => v.command === 'request_userguide_profile')")
            page.evaluate('(models) => HandleStudio({command: "response_userguide_profile", response: {model: models}})', models)
            expect(page.locator('.PrinterBlock')).to_have_count(len(models))
            for name in ('Bambu Lab A1', 'Bambu Lab P2S'):
                checkbox = page.locator(f'.ModelCheckBox[vendor="BBL"][model="{name}"]')
                expect(checkbox).to_have_count(1)
                checkbox.click()
                expect(checkbox).to_have_class('ModelCheckBox ModelCheckBoxSelected')
            if args.screenshot:
                args.screenshot.parent.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(args.screenshot))
            page.locator('#AcceptBtn').click()
            commands = page.evaluate('window.__commands')
            saved = next(v for v in commands if v['command'] == 'save_userguide_models')
            assert set(saved['data']) == {'Bambu Lab A1', 'Bambu Lab P2S'}, saved
            assert any(v['command'] == 'request_page_after_models' for v in commands)
            assert not errors, errors
            browser.close()
            print(f'PASS: {len(models)} printer cards render; A1/P2S selectable; Next saves selection and requests next step')
            print('PASS: old cache-only layout is rejected. Native bridge mocked; no native Windows GUI verification.')
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == '__main__':
    main()
