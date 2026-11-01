#!/usr/bin/env python3
"""Exercise the real home-page DOM and login buttons with a mocked native bridge.

This is automated browser verification, not native Windows GUI or live account
authentication. All network access is restricted to the local test server.
Requires playwright; uses an installed Chromium/Chrome browser.
"""
import argparse
import functools
import http.server
from pathlib import Path
import shutil
import threading
from urllib.parse import urlsplit

from playwright.sync_api import expect, sync_playwright


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--screenshot', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    browser_path = next((shutil.which(name) for name in
                         ('chromium', 'chromium-browser', 'google-chrome')
                         if shutil.which(name)), None)
    if not browser_path:
        raise SystemExit('An installed Chromium or Chrome browser is required')

    handler = functools.partial(QuietHandler, directory=str(root / 'resources/web'))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    origin = f'http://127.0.0.1:{server.server_port}'
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(executable_path=browser_path)
            context = browser.new_context(user_agent='BBL-Slicer login-regression',
                                          viewport={'width': 1150, 'height': 850})
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
            page.goto(origin + '/homepage/index.html')

            def message(command, **fields):
                page.evaluate('(value) => HandleStudio(value)', {'command': command, **fields})

            def last_command():
                return page.evaluate('window.__commands.at(-1).command')

            expect(page.locator('#LeftBoard')).to_be_hidden()
            message('cloud_providers_info', data={'providers': ['orca']})
            expect(page.locator('#OrcaAccount')).to_be_visible()
            expect(page.locator('#OrcaAccount .AccountSignIn')).to_have_text('Login')
            expect(page.locator('#BambuCloudSection')).to_be_hidden()
            page.locator('#OrcaAccount').click()
            assert last_command() == 'homepage_orca_login_or_register'

            message('cloud_providers_info', data={'providers': ['orca', 'bbl']})
            expect(page.locator('#BambuCloudHeader')).to_be_visible()
            page.locator('#BambuCloudHeader').click()
            expect(page.locator('#BambuAccount')).to_be_visible()
            page.locator('#BambuAccount').click()
            assert last_command() == 'homepage_bambu_login_or_register'

            message('network_plugin_installtip', show=1)
            expect(page.locator('#NoPluginTip')).to_be_visible()
            before = page.evaluate('window.__commands.length')
            page.locator('#BambuAccount').click(force=True)
            assert page.evaluate('window.__commands.length') == before
            message('network_plugin_installtip', show=0)

            message('orca_userlogin', data={'avatar': '', 'name': 'Orca Test', 'account': 'test-account'})
            message('studio_bambu_userlogin', data={'avatar': '', 'name': 'Bambu Test'})
            expect(page.locator('#UserName')).to_have_text('Orca Test')
            expect(page.locator('#BambuUserName')).to_have_text('Bambu Test')
            if args.screenshot:
                args.screenshot.parent.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(args.screenshot))

            for row, command in [('OrcaAccount', 'homepage_orca_logout'),
                                 ('BambuAccount', 'homepage_bambu_logout')]:
                page.locator('#' + row).click()
                expect(page.locator('#AccountMenu')).to_have_class('Open')
                page.locator('#AccountMenuLogout').click()
                assert last_command() == command

            page.locator('[menu="orca_cloud"]').click()
            assert last_command() == 'homepage_open_cloud'
            page.locator('[menu="recent"]').click()
            assert last_command() == 'homepage_close_cloud'

            message('cloud_providers_info', data={'providers': []})
            message('orca_userlogin', data={'avatar': '', 'name': 'Late callback', 'account': ''})
            page.evaluate('SetLoginPanelVisibility(true)')
            expect(page.locator('#LeftBoard')).to_be_hidden()
            assert not errors, errors
            browser.close()
            print('PASS: Orca/Bambu login buttons, account menus, plugin gating, cloud navigation and hidden-panel state')
            print('Mocked native bridge; no real sign-in or native Windows GUI verification performed.')
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == '__main__':
    main()
