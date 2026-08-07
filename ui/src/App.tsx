import { useState, useEffect } from 'react';
import Navbar from './components/Navbar';
import EditorPanel from './components/EditorPanel';
import OutputPanel from './components/OutputPanel';
// 1. Importiamo le funzioni normali
import { submitCode, listenForJobStatus } from './services/apiClient';

// 2. Importiamo l'interfaccia specificando "type"
import type { JobStatusUpdate } from './services/apiClient';

// Esportiamo il tipo così potrai usarlo anche negli altri componenti se necessario
export type CompileStatus = 'idle' | 'submitting' | 'queued' | 'compiling' | 'completed' | 'error';
export type ThemeMode = 'dark' | 'light';

function App() {
  // 1. Cronologia Locale: Inizializziamo il codice da localStorage o usiamo il default
  const [code, setCode] = useState<string>(() => {
    const savedCode = localStorage.getItem('compileforge_code');
    if (savedCode !== null) return savedCode;
    return `int main() {\n    int a = 5;\n    int b = 10;\n    return a + b;\n}`;
  });

  // 2. Stato del Tema (Dark/Light)
  const [theme, setTheme] = useState<ThemeMode>(() => {
    const savedTheme = localStorage.getItem('compileforge_theme') as ThemeMode;
    return savedTheme || 'dark';
  });
  
  // Nuovi stati per la gestione del flusso
  const [status, setStatus] = useState<CompileStatus>('idle');
  const [output, setOutput] = useState<string>('');
  const [assembly, setAssembly] = useState<string>('');
  const [irCode, setIrCode] = useState<string>('');
  const [metrics, setMetrics] = useState<{start?: string, end?: string}>({});
  const [downloadUrl, setDownloadUrl] = useState<string | null>(null);
  const [currentJobId, setCurrentJobId] = useState<string | null>(null);

  // Salvataggio automatico del codice nel localStorage a ogni modifica
  useEffect(() => {
    localStorage.setItem('compileforge_code', code);
  }, [code]);

  // Salvataggio automatico del tema e applicazione preferenza
  useEffect(() => {
    localStorage.setItem('compileforge_theme', theme);
  }, [theme]);

  // Funzione per invertire il tema
  const toggleTheme = () => {
    setTheme(prev => (prev === 'dark' ? 'light' : 'dark'));
  };

  // Funzione per gestire la selezione dei snippet precaricati
  const handleSelectSnippet = (snippetCode: string) => {
    setCode(snippetCode);
  };

  // 1. Funzione che verrà passata alla Navbar e associata al bottone "Compila"
  const handleCompile = async () => {
    try {
      setStatus('submitting');
      setOutput(''); 
      setAssembly('');
      setIrCode('');
      setMetrics({});
      setDownloadUrl(null); 
      setCurrentJobId(null); 
      
      const jobId = await submitCode(code);
      setCurrentJobId(jobId); 
      
    } catch (err) {
      console.error(err);
      setStatus('error');
      setOutput(err instanceof Error ? err.message : 'Errore sconosciuto durante l\'invio al Gateway.');
    }
  };

  // 2. L'hook per ascoltare i Server-Sent Events
  useEffect(() => {
    if (!currentJobId) return;

    setStatus('queued');
    
    const cleanupSSE = listenForJobStatus(
      currentJobId,
      (data: JobStatusUpdate) => {
        if (data.status === 'processing') {
          setStatus('compiling');
          if (data.message) setOutput(prev => prev + data.message + '\n');
        } 
        else if (data.status === 'completed') {
          setStatus('completed');
          if (data.output) setOutput(data.output);
          
          if (data.assembly) setAssembly(data.assembly);
          if (data.irCode) setIrCode(data.irCode);
          if (data.downloadUrl) setDownloadUrl(data.downloadUrl);
          setMetrics({ start: data.createdAt, end: data.finishedAt });
        }
        else if (data.status === 'failed') {
          setStatus('error');
          if (data.errorMessage) setOutput(`Errore di Compilazione:\n${data.errorMessage}`);
        }
      },
      (_error: any) => {
        setStatus('error');
        setOutput((prev: string) => prev + '\n[Errore di rete] Connessione al server interrotta.');
      }
    );

    return () => {
      cleanupSSE();
    };
  }, [currentJobId]);

  // Gestione classi dinamiche in base al tema
  const isDark = theme === 'dark';

  return (
    <div className={`h-screen w-screen flex flex-col overflow-hidden ${isDark ? 'bg-gray-900 text-white' : 'bg-gray-100 text-gray-900'}`}>
      {/* Navbar aggiornata con controllo Tema e Snippet */}
      <Navbar 
        onCompile={handleCompile} 
        status={status} 
        theme={theme} 
        onToggleTheme={toggleTheme}
        onSelectSnippet={handleSelectSnippet}
      />
      
      <div className="flex flex-1 overflow-hidden">
        <div className={`w-1/2 ${isDark ? 'border-r border-gray-700' : 'border-r border-gray-300'}`}>
          <EditorPanel code={code} setCode={setCode} theme={theme} />
        </div>
        <div className="w-1/2">
          <OutputPanel 
            output={output} 
            status={status} 
            assembly={assembly}
            irCode={irCode}
            metrics={metrics}
            downloadUrl={downloadUrl}
            theme={theme}
          />
        </div>
      </div>
    </div>
  );
}

export default App;