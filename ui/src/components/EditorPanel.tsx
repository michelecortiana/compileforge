import { useRef } from 'react';
import type { ChangeEvent, DragEvent } from 'react';
import Editor from '@monaco-editor/react';

interface EditorPanelProps {
  code: string;
  setCode: (value: string) => void;
}

const EditorPanel = ({ code, setCode }: EditorPanelProps) => {
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Gestisce la scrittura manuale nell'editor
  const handleEditorChange = (value: string | undefined) => {
    if (value !== undefined) {
      setCode(value);
    }
  };

  // Funzione base per leggere il contenuto di un file
  const readFile = (file: File) => {
    const reader = new FileReader();
    reader.onload = (e) => {
      const content = e.target?.result as string;
      setCode(content);
    };
    reader.readAsText(file);
  };

  // Gestisce il click sul pulsante "Carica File"
  const handleFileUpload = (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      readFile(file);
    }
    // Resetta l'input per permettere di ricaricare lo stesso file
    if (event.target) {
      event.target.value = ''; 
    }
  };

  // Gestisce l'evento di rilascio del drag & drop
  const handleDrop = (event: DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    const file = event.dataTransfer.files?.[0];
    if (file && (file.name.endsWith('.c') || file.name.endsWith('.txt'))) {
      readFile(file);
    } else {
      alert("Formato non supportato. Per favore, carica un file .c");
    }
  };

  // Previene il comportamento di default per consentire il drop
  const handleDragOver = (event: DragEvent<HTMLDivElement>) => {
    event.preventDefault();
  };

  return (
    <div 
      className="flex flex-col h-full w-full bg-gray-900"
      onDrop={handleDrop}
      onDragOver={handleDragOver}
    >
      {/* Toolbar superiore dell'Editor */}
      <div className="flex justify-between items-center px-4 py-2 bg-gray-800 border-b border-gray-700">
        <span className="text-sm font-mono text-gray-400">main.c</span>
        
        <div>
          {/* Input file nascosto */}
          <input 
            type="file" 
            accept=".c,.txt" 
            ref={fileInputRef} 
            onChange={handleFileUpload} 
            className="hidden" 
          />
          <button 
            onClick={() => fileInputRef.current?.click()}
            className="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 rounded text-gray-200 transition-colors border border-gray-600"
          >
            Carica File .c
          </button>
        </div>
      </div>

      {/* Monaco Editor */}
      <div className="flex-1">
        <Editor
          height="100%"
          defaultLanguage="c"
          theme="vs-dark"
          value={code}
          onChange={handleEditorChange}
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