import { useRef, useEffect, useState } from 'react';
import type { ChangeEvent, DragEvent } from 'react';
import Editor, { useMonaco } from '@monaco-editor/react';
import type { ThemeMode, CompileStatus } from '../App';

interface EditorPanelProps {
  code: string;
  setCode: (value: string) => void;
  theme: ThemeMode;
  status?: CompileStatus;
  output?: string;
}

const MAX_FILE_SIZE_BYTES = 95000;

const EditorPanel = ({ code, setCode, theme, status, output }: EditorPanelProps) => {
  const fileInputRef = useRef<HTMLInputElement>(null);
  const editorRef = useRef<any>(null);
  const monaco = useMonaco();
  const isDark = theme === 'dark';
  
  const [unsupportedWarning, setUnsupportedWarning] = useState<string | null>(null);

  const handleEditorDidMount = (editor: any) => {
    editorRef.current = editor;
  };

  useEffect(() => {
    if (!monaco || !editorRef.current) return;
    
    const model = editorRef.current.getModel();
    if (!model) return;

    if (status === 'failed' || status === 'error') {
      const markers: any[] = [];
      const lines = output?.split('\n') || [];

      lines.forEach(line => {
        const match = line.match(/(?:\(|alla\s+)?riga\s+(\d+),\s*colonna\s+(\d+)\)?/i);
        
        if (match) {
          const lineNum = parseInt(match[1], 10);
          const colNum = parseInt(match[2], 10);
          const msg = line.replace(match[0], '').trim();

          markers.push({
            startLineNumber: lineNum,
            startColumn: colNum,
            endLineNumber: lineNum,
            endColumn: colNum + 6,
            message: msg,
            severity: monaco.MarkerSeverity.Error
          });
        }
      });

      monaco.editor.setModelMarkers(model, 'compiler', markers);
    } else {
      monaco.editor.setModelMarkers(model, 'compiler', []);
    }
  }, [status, output, monaco]);

  const checkUnsupportedConstructs = (text: string) => {
    const unsupported = [];
    
    let cleanText = text.replace(/\/\/.*$/gm, ''); 
    cleanText = cleanText.replace(/"[^"]*"/g, '');  

    if (/#(include|define|ifdef|pragma)\b/.test(cleanText)) unsupported.push('Preprocessore (#)');
    if (/\/\*/.test(text)) unsupported.push('Commenti multilinea (/* */)');
    
    if (/\bunsigned\b/.test(cleanText)) unsupported.push('Tipi unsigned');
    if (/\b(bool|_Bool|true|false)\b/.test(cleanText)) unsupported.push('Booleani');
    if (/\b(enum|typedef|union)\b/.test(cleanText)) unsupported.push('Tipi custom (enum/typedef/union)');
    if (/\b(const|static|volatile)\b/.test(cleanText)) unsupported.push('Qualificatori (const/static/volatile)');
    if (/\bvoid\b/.test(cleanText)) unsupported.push('Tipo void');
    
    if (/\[\s*[^\]]*\s*\]\s*\[/.test(cleanText)) unsupported.push('Array multidimensionali');
    
    if (/\+\+/.test(cleanText)) unsupported.push('Operatore ++');
    if (/\+=/.test(cleanText)) unsupported.push('Operatore +=');
    if (/\?/.test(cleanText)) unsupported.push('Operatore ternario (?:)');
    if (/<<|>>|\^|\|(?!=|\|)/.test(cleanText)) unsupported.push('Operatori bit a bit');
    if (/!(?!=)/.test(cleanText)) unsupported.push('NOT logico standalone (!)');
    
    if (/\b(switch|case|do|goto)\b/.test(cleanText)) unsupported.push('switch/case/do/goto');
    
    if (/\b\d+(\.\d+)?[fFuUlL]+\b/.test(cleanText)) unsupported.push('Suffissi numerici (es. 1.5f)');
    
    if (unsupported.length > 0) {
      const uniqueUnsupported = Array.from(new Set(unsupported));
      setUnsupportedWarning(`⚠️ Attenzione: Il compilatore attualmente non supporta: ${uniqueUnsupported.join(', ')}`);
    } else {
      setUnsupportedWarning(null);
    }
  };

  useEffect(() => {
    checkUnsupportedConstructs(code);
  }, [code]);

  const handleEditorChange = (value: string | undefined) => {
    if (value !== undefined) {
      setCode(value);
      
      checkUnsupportedConstructs(value);
      
      if (monaco && editorRef.current) {
        const model = editorRef.current.getModel();
        if (model) {
          monaco.editor.setModelMarkers(model, 'compiler', []);
        }
      }
    }
  };

  const readFile = (file: File) => {
    const reader = new FileReader();
    reader.onload = (e) => {
      const content = e.target?.result as string;
      setCode(content);
      
      checkUnsupportedConstructs(content);
      
      if (monaco && editorRef.current) {
        const model = editorRef.current.getModel();
        if (model) {
          monaco.editor.setModelMarkers(model, 'compiler', []);
        }
      }
    };
    reader.readAsText(file);
  };

  const handleFileUpload = (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      if (file.size > MAX_FILE_SIZE_BYTES) {
        alert("⚠️ Il file è troppo grande! La dimensione massima consentita è ~95KB.");
      } else {
        readFile(file);
      }
    }
    if (event.target) {
      event.target.value = ''; 
    }
  };

  const handleDrop = (event: DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    const file = event.dataTransfer.files?.[0];
    
    if (file) {
      if (file.size > MAX_FILE_SIZE_BYTES) {
        alert("⚠️ Il file è troppo grande! La dimensione massima consentita è ~95KB.");
        return;
      }
      if (file.name.endsWith('.c') || file.name.endsWith('.txt')) {
        readFile(file);
      } else {
        alert("Formato non supportato. Per favore, carica un file .c");
      }
    }
  };

  const handleDragOver = (event: DragEvent<HTMLDivElement>) => {
    event.preventDefault();
  };

  return (
    <div className={`flex flex-col h-full w-full ${isDark ? 'bg-gray-900' : 'bg-white'}`} onDrop={handleDrop} onDragOver={handleDragOver}>
      <div className={`flex justify-between items-center px-4 py-2 border-b ${isDark ? 'bg-gray-800 border-gray-700' : 'bg-gray-50 border-gray-200'}`}>
        <span className={`text-sm font-mono ${isDark ? 'text-gray-400' : 'text-gray-600'}`}>main.c</span>
        
        <div>
          <input type="file" accept=".c,.txt" ref={fileInputRef} onChange={handleFileUpload} className="hidden" />
          <button 
            onClick={() => fileInputRef.current?.click()}
            className={`px-3 py-1.5 text-sm rounded transition-colors border ${isDark ? 'bg-gray-700 hover:bg-gray-600 text-gray-200 border-gray-600' : 'bg-white hover:bg-gray-100 text-gray-700 border-gray-300 shadow-sm'}`}
          >
            Carica File .c
          </button>
        </div>
      </div>

      {unsupportedWarning && (
        <div className="bg-amber-500 text-amber-950 px-4 py-2 text-sm font-semibold flex justify-between items-center animate-pulse">
          <span>{unsupportedWarning}</span>
          <button 
            onClick={() => setUnsupportedWarning(null)} 
            className="text-amber-950 hover:text-black hover:scale-110 transition-transform font-bold"
            title="Chiudi avviso"
          >
            ✕
          </button>
        </div>
      )}

      <div className="flex-1">
        <Editor
          height="100%"
          defaultLanguage="c"
          theme={isDark ? "vs-dark" : "vs"} 
          value={code}
          onChange={handleEditorChange}
          onMount={handleEditorDidMount}
          options={{
            minimap: { enabled: false },
            fontSize: 15,
            wordWrap: 'on',
            scrollBeyondLastLine: false,
            padding: { top: 16 },
            fontFamily: "'Fira Code', 'Courier New', monospace",
          }}
        />
      </div>
    </div>
  );
};

export default EditorPanel;