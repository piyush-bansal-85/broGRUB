
const vscode = require('vscode');
const descriptions = require("./data/osInfo.js");

const osList = [
    ['gnu-linux', 'Generic GNU/Linux menu class'],
    ['gnu', 'Generic GNU menu class'],
    ['ubuntu', 'Ubuntu GNU/Linux'],
    ['debian', 'Debian GNU/Linux'],
    ['fedora', 'Fedora Linux'],
    ['arch', 'Arch Linux'],
    ['windows', 'Microsoft Windows'],
    ['osx', 'Apple macOS'],
    ['darwin', 'Apple macOS / Darwin'],
    ['hurd', 'GNU Hurd'],
    ['os', 'Generic operating-system menu class'],
    ['others', 'Fallback key for other operating systems']
];

function activate(context) {
    // Hover information for OS keys.
    const hoverProvider = vscode.languages.registerHoverProvider('bro', {
        provideHover(document, position) {
            const line = document.lineAt(position.line).text;

            if (line.trimStart().startsWith('#')) {
                return;
            }

            const match = line.match(/^\s*([A-Za-z0-9_-]+)\s*:/);

            if (!match) {
                return;
            }

            const key = match[1];
            const keyStart = line.indexOf(key);

            if (
                position.character < keyStart ||
                position.character >= keyStart + key.length
            ) {
                return;
            }

            const description = descriptions[key.toLowerCase()];

            if (!description) {
                return;
            }

            const markdown = new vscode.MarkdownString(description);
            markdown.isTrusted = false;

            return new vscode.Hover(
                markdown,
                new vscode.Range(
                    position.line,
                    keyStart,
                    position.line,
                    keyStart + key.length
                )
            );
        }
    });

    // OS key autocomplete.
    const completionProvider =
        vscode.languages.registerCompletionItemProvider(
            'bro',
            {
                provideCompletionItems(document, position) {
                    const line = document.lineAt(position.line).text;
                    const beforeCursor = line.slice(0, position.character);

                    // No suggestions inside comments.
                    if (beforeCursor.includes('#')) {
                        return [];
                    }

                    // No OS suggestions after the key/value separator.
                    if (beforeCursor.includes(':')) {
                        return [];
                    }

                    // Only suggest keys at the start of a line.
                    const match = beforeCursor.match(
                        /^(\s*)([A-Za-z0-9_-]*)$/
                    );

                    if (!match) {
                        return [];
                    }

                    const prefix = match[2].toLowerCase();
                    const wordStart =
                        position.character - match[2].length;

                    return osList
                        .filter(([key]) =>
                            key.toLowerCase().startsWith(prefix)
                        )
                        .map(([key, description]) => {
                            const item = new vscode.CompletionItem(
                                key,
                                vscode.CompletionItemKind.Enum
                            );

                            item.detail = description;
                            item.documentation =
                                new vscode.MarkdownString(
                                    `BroGRUB OS key: \`${key}\`\n\n${description}`
                                );

                            item.filterText = key;
                            item.range = new vscode.Range(
                                position.line,
                                wordStart,
                                position.line,
                                position.character
                            );

                            return item;
                        });
                }
            },
            ...'abcdefghijklmnopqrstuvwxyz'.split('')
        );

    context.subscriptions.push(hoverProvider, completionProvider);
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
