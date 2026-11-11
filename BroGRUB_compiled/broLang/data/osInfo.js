module.exports = {
    ubuntu: [
        '### Ubuntu',
        'A Linux distribution based on Debian.',
        '',
        '**BroGRUB key:** `ubuntu`',
        '**Purpose:** Selects the icon associated with Ubuntu.',
        '',
        'Example: `ubuntu:icons/Ubuntu_GRUB_blur.png`'
    ].join('\n'),

    windows: [
        '### Windows',
        'Microsoft Windows operating system.',
        '',
        '**BroGRUB key:** `windows`',
        '**Purpose:** Selects the icon associated with Windows.',
        '',
        'Example: `windows:icons/Window_GRUB_blur.png`'
    ].join('\n'),

    others: [
        '### Other operating systems',
        'Fallback entry for other OS entries, as handled by BroGRUB.',
        '',
        '**BroGRUB key:** `others`',
        '**Purpose:** Supplies the shared icon for other OS entries.',
        '',
        'Example: `others:icons/others_blur.png`'
    ].join('\n')
};