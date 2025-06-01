" =============================================================================
" My Complete .vimrc Configuration with Custom Statusline
" =============================================================================

" ---------- Basic Settings ----------
" Turn off Vi compatibility mode to enable enhanced Vim features
set nocompatible

" Enable file type detection, plugins, and automatic indenting based on file type
filetype plugin indent on

" Enable syntax highlighting for improved readability
syntax on

" ---------- Visual Appearance ----------
" Highlight the current line to easily track the cursor position
set cursorline

" Display absolute line numbers
set number

" Display line numbers relative to the current line for easier navigation
set relativenumber

" Show normally invisible characters (for example, tabs, trailing spaces, etc.)
set list
" Customize the symbols used for invisible characters:
"   eol: End-of-line marker, tab: Tabs, trail: Trailing spaces,
"   extends: When text goes off to the right, precedes: off to the left,
"   space: visible space, nbsp: non-breaking space.
set listchars=eol:¬,tab:>·,trail:~,extends:>,precedes:<,space:␣,nbsp:.

" Enable dynamic window title based on the file being edited
set title

" ---------- Indentation ----------
" Enable smart indentation so that new lines auto-indent appropriately
set smartindent

" ---------- Colors and Themes ----------
" Load the 'sorbet' color scheme (ensure it’s installed or change to one you prefer)
color sorbet

" Customize the 'Normal' highlight group:
"   ctermbg=16 sets a dark terminal background;
"   guibg=#000000 sets the GUI background to pure black.
hi Normal ctermbg=16 guibg=#000000

" -----------------------------------------------------------------------------
" Always show the status line (even if only one window is open)
set laststatus=2

" =============================================================================
" Custom Statusline Setup (Using += to Append Components)
" =============================================================================

" Clear any existing statusline contents
set statusline=

" --- Left Section ---
" 1. File name relative to the current directory
set statusline+=%f

" 2. Flags:
"    %m -> modified flag,
"    %r -> read-only flag,
"    %h -> help file flag,
"    %w -> preview window flag.
set statusline+=%m%r%h%w

" 3. File type (syntax) shown within square brackets
set statusline+=\ [%Y]

" 4. Additional hex info (if needed) in square brackets; adjust or remove if unwanted
set statusline+=\ [0x%02.2B]

" 5. A truncation indicator if the statusline becomes too long
set statusline+=%<

" 6. The full file name (if space permits)
set statusline+=\ %F

" --- Divider between Left and Right Sections ---
" Everything before %= is left-aligned; everything after is right-aligned.
set statusline+=%=

" --- Right Section (Cursor Information) ---
" Here, we format the statusline to display:
"    Col:[current column]  Line:[current line]/[total lines]  [percentage]%"
" Note: %c displays the current column,
"       %l displays the current (absolute) line number,
"       %L displays the total number of lines,
"       %p displays the percentage through the file.
set statusline+=\ Col:%c\  
set statusline+=\ Line:%l/%L\  
set statusline+=\ %p%%\  

" =============================================================================
" Optional Additional Settings
" =============================================================================

" Create a backup file when writing (optional)
set backup

" Enable persistent undo so that undo history is saved between sessions
set undofile

" Enable incremental search highlighting as you type
set incsearch

" Highlight search matches throughout the file
set hlsearch

" -----------------------------------------------------------------------------
" Key Mappings for Easier Window Navigation
" Use Ctrl-h/j/k/l to move between splits
nnoremap <C-h> <C-w>h
nnoremap <C-j> <C-w>j
nnoremap <C-k> <C-w>k
nnoremap <C-l> <C-w>l

" =============================================================================
" End of .vimrc
" =============================================================================

