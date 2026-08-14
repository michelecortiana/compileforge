import { useState, useEffect } from 'react';
import Navbar from './components/Navbar';
import EditorPanel from './components/EditorPanel';
import OutputPanel from './components/OutputPanel';
// 👇 Importa il nuovo modale
import LimitationsModal from './components/LimitationsModal';
import { submitCode, listenForJobStatus, fetchRecentJobs, fetchJobDetails } from './services/apiClient';
import type { JobStatusUpdate } from './services/apiClient';

export type CompileStatus = 'idle' | 'submitting' | 'pending' | 'processing' | 'compiling' | 'completed' | 'failed' | 'error' | 'connected';
export type ThemeMode = 'dark' | 'light';

function App() {
  const [code, setCode] = useState<string>(() => {
    const savedCode = localStorage.getItem('compileforge_code');
    if (savedCode !== null) return savedCode;
    return `int main() {\n    int a = 5;\n    int b = 10;\n    return a + b;\n}`;
  });

  const [theme, setTheme] = useState<ThemeMode>(() => {
    const savedTheme = localStorage.getItem('compileforge_theme') as ThemeMode;
    return savedTheme || 'dark';
  });
  
  const [status, setStatus] = useState<CompileStatus>('idle');
  const [output, setOutput] = useState<string>('');
  const [assembly, setAssembly] = useState<string>('');
  const [irCode, setIrCode] = useState<string>('');
  const [metrics, setMetrics] = useState<{start?: string, end?: string}>({});
  const [currentJobId, setCurrentJobId] = useState<string | null>(null);

  // === STATO MODALE STORICO E INFO ===
  const [isHistoryOpen, setIsHistoryOpen] = useState(false);
  const [historyJobs, setHistoryJobs] = useState<JobStatusUpdate[]>([]);
  const [isLoadingHistory, setIsLoadingHistory] = useState(false);
  
  // 👇 STATO PER IL MODALE INFO (impostato a true per aprirlo all'avvio)
  const [showInfoModal, setShowInfoModal] = useState(true);

  useEffect(() => {
    localStorage.setItem('compileforge_code', code);
  }, [code]);

  useEffect(() => {
    localStorage.setItem('compileforge_theme', theme);
  }, [theme]);

  const toggleTheme = () => setTheme(prev => (prev === 'dark' ? 'light' : 'dark'));
  const handleSelectSnippet = (snippetCode: string) => setCode(snippetCode);

  const handleCompile = async () => {
    try {
      setStatus('submitting');
      setOutput(''); 
      setAssembly('');
      setIrCode('');
      setMetrics({});
      setCurrentJobId(null); 
      
      const jobId = await submitCode(code);
      setCurrentJobId(jobId); 
      window.history.pushState({}, '', `/j/${jobId}`);
      
    } catch (err) {
      console.error(err);
      setStatus('error');
      setOutput(err instanceof Error ? err.message : 'Errore sconosciuto durante l\'invio al Gateway.');
    }
  };

  useEffect(() => {
    if (!currentJobId) return;
    setStatus('pending');
    
    const cleanupSSE = listenForJobStatus(
      currentJobId,
      (data: JobStatusUpdate) => {
        setStatus(data.status as CompileStatus);

        if (data.status === 'processing') {
          if (data.message) setOutput(prev => prev + data.message + '\n');
        } 
        else if (data.status === 'completed') {
          if (data.output) setOutput(data.output);
          if (data.assembly) setAssembly(data.assembly);
          if (data.irCode) setIrCode(data.irCode);
          setMetrics({ start: data.createdAt, end: data.finishedAt });
        }
        else if (data.status === 'failed') {
          if (data.errorMessage) setOutput(`Errore di Compilazione:\n${data.errorMessage}`);
        }
      },
      (_error: any) => {
        setStatus('error');
        setOutput((prev: string) => prev + '\n[Errore di rete] Connessione al server interrotta.');
      }
    );

    return () => cleanupSSE();
  }, [currentJobId]);

  // === LOGICA CONDIVISIONE LINK ===
  useEffect(() => {
    const path = window.location.pathname;
    const match = path.match(/^\/j\/([a-zA-Z0-9-]+)$/);
    if (match && match[1]) {
      const jobIdFromUrl = match[1];
      loadJob(jobIdFromUrl);
    }
  }, []);

  // === LOGICA STORICO JOB ===
  const openHistory = async () => {
    setIsHistoryOpen(true);
    setIsLoadingHistory(true);
    try {
      const jobs = await fetchRecentJobs(20);
      setHistoryJobs(jobs);
    } catch (error) {
      console.error("Errore caricamento storico:", error);
    } finally {
      setIsLoadingHistory(false);
    }
  };

  const loadJob = async (id: string) => {
    try {
      setIsHistoryOpen(false);
      setStatus('connected');
      setOutput('Recupero dettagli job dal server...');
      
      const jobDetails = await fetchJobDetails(id);
      
      if (jobDetails.sourceCode) setCode(jobDetails.sourceCode);
      setCurrentJobId(jobDetails.id || id);
      setStatus(jobDetails.status as CompileStatus);

      window.history.pushState({}, '', `/j/${jobDetails.id || id}`);

      if (jobDetails.status === 'completed') {
        setOutput(jobDetails.output || '');
        setAssembly(jobDetails.assembly || '');
        setIrCode(jobDetails.irCode || '');
        setMetrics({ start: jobDetails.createdAt, end: jobDetails.finishedAt });
      } else if (jobDetails.status === 'failed') {
        setOutput(`Errore di Compilazione:\n${jobDetails.errorMessage || ''}`);
        setAssembly('');
        setIrCode('');
        setMetrics({ start: jobDetails.createdAt, end: jobDetails.finishedAt });
      }
    } catch (error) {
      console.error("Errore caricamento dettagli job:", error);
      alert("Impossibile caricare i dettagli del job.");
    }
  };

  const isDark = theme === 'dark';

  return (
    <div className={`h-screen w-screen flex flex-col overflow-hidden relative ${isDark ? 'bg-gray-900 text-white' : 'bg-gray-100 text-gray-900'}`}>
      <Navbar 
        onCompile={handleCompile} 
        status={status} 
        theme={theme} 
        onToggleTheme={toggleTheme}
        onSelectSnippet={handleSelectSnippet}
        onOpenHistory={openHistory}
        onOpenInfo={() => setShowInfoModal(true)} // 👈 PASSA LA FUNZIONE ALLA NAVBAR
      />
      
      <div className="flex flex-1 overflow-hidden">
        <div className={`w-1/2 ${isDark ? 'border-r border-gray-700' : 'border-r border-gray-300'}`}>
          <EditorPanel code={code} setCode={setCode} theme={theme} status={status} output={output} />
        </div>
        <div className="w-1/2">
          <OutputPanel 
            output={output} 
            status={status} 
            assembly={assembly}
            irCode={irCode}
            metrics={metrics}
            jobId={currentJobId}
            theme={theme}
          />
        </div>
      </div>

      {/* COMPONENTE MODALE STORICO */}
      {isHistoryOpen && (
        <div className="absolute inset-0 z-50 flex items-center justify-center bg-black bg-opacity-60">
          <div className={`w-3/4 max-w-3xl rounded-lg shadow-xl flex flex-col max-h-[80vh] ${isDark ? 'bg-gray-800 border border-gray-700' : 'bg-white'}`}>
            <div className={`flex justify-between items-center p-4 border-b ${isDark ? 'border-gray-700' : 'border-gray-200'}`}>
              <h2 className="text-xl font-bold">Storico Job Recenti</h2>
              <button onClick={() => setIsHistoryOpen(false)} className="text-2xl font-bold leading-none hover:text-red-500">&times;</button>
            </div>
            
            <div className="p-4 overflow-y-auto flex-1">
              {isLoadingHistory ? (
                <p className="text-center py-4">Caricamento storico in corso...</p>
              ) : historyJobs.length === 0 ? (
                <p className="text-center py-4 text-gray-500">Nessun job trovato.</p>
              ) : (
                <ul className="space-y-2">
                  {historyJobs.map(job => (
                    <li 
                      key={job.id} 
                      className={`p-3 rounded border flex justify-between items-center cursor-pointer transition-colors ${
                        isDark ? 'border-gray-700 hover:bg-gray-700' : 'border-gray-300 hover:bg-gray-50'
                      }`}
                      onClick={() => loadJob(job.id!)}
                    >
                      <div>
                        <p className="font-mono text-sm">{job.id}</p>
                        <p className="text-xs text-gray-500">{new Date(job.createdAt!).toLocaleString()}</p>
                      </div>
                      <div>
                        {job.status === 'completed' && <span className="text-green-500 font-bold">Completato</span>}
                        {job.status === 'failed' && <span className="text-red-500 font-bold">Fallito</span>}
                        {(job.status !== 'completed' && job.status !== 'failed') && <span className="text-yellow-500 font-bold">{job.status}</span>}
                      </div>
                    </li>
                  ))}
                </ul>
              )}
            </div>
          </div>
        </div>
      )}

      {/* 👇 NUOVO MODALE LIMITAZIONI */}
      {showInfoModal && (
        <LimitationsModal 
          onClose={() => setShowInfoModal(false)}
          theme={theme}
        />
      )}
    </div>
  );
}

export default App;