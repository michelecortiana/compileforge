import type { CompileRequest, CompileResponse, ErrorResponse } from '../types/api';

export type { CompileRequest, CompileResponse, ErrorResponse };

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL;
const API_KEY = import.meta.env.VITE_API_KEY;

export async function submitCode(sourceCode: string): Promise<string> {
  try {
    const response = await fetch(`${API_BASE_URL}/compile`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': API_KEY,
      },
      body: JSON.stringify({
        source_code: sourceCode,
        language: 'c',
      }),
    });

    if (!response.ok) {
      const errorData = await response.json().catch(() => ({})) as Partial<ErrorResponse>;
      throw new Error(errorData.error || `Errore HTTP: ${response.status}`);
    }

    const data: CompileResponse = await response.json();
    return data.job_id;
    
  } catch (error) {
    console.error("Errore durante la comunicazione col Gateway:", error);
    throw error;
  }
}

export interface JobStatusUpdate {
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'connected' | 'error';
  message?: string;
  output?: string;
  errorMessage?: string;
  assembly?: string;
  irCode?: string;
  createdAt?: string;
  finishedAt?: string;
  downloadUrl?: string; 
  sourceCode?: string;
  id?: string;
}

export function listenForJobStatus(
  jobId: string,
  onUpdate: (data: JobStatusUpdate) => void,
  onError: (error: Event) => void
): () => void {
  const streamUrl = `${API_BASE_URL}/status/${jobId}/stream?key=${API_KEY}`;
  let eventSource: EventSource | null = new EventSource(streamUrl);
  let isClosedManually = false;
  let pollingInterval: any = null;

  // Funzione di supporto per pulire tutto
  const cleanup = () => {
    isClosedManually = true;
    if (eventSource && eventSource.readyState !== EventSource.CLOSED) {
      eventSource.close();
    }
    if (pollingInterval) {
      clearInterval(pollingInterval);
    }
  };

  eventSource.onmessage = (event) => {
    try {
      const data: JobStatusUpdate = JSON.parse(event.data);
      onUpdate(data);

      if (data.status === 'completed' || data.status === 'failed') {
        cleanup();
      }
    } catch (err) {
      console.error("Errore nel parsing del messaggio SSE:", err);
    }
  };

  eventSource.onerror = (error) => {
    console.warn("⚠️ Connessione SSE caduta. Avvio fallback di polling...", error);
    
    // Chiudiamo la SSE morta
    if (eventSource) {
      eventSource.close();
    }

    if (isClosedManually) return;

    // --- STRATEGIA DI POLLING DI FALLBACK ---
    let attempts = 0;
    const maxAttempts = 20; // Es. prova per 20 volte (ogni 2 secondi = 40 secondi totali)

    pollingInterval = setInterval(async () => {
      if (isClosedManually) {
        clearInterval(pollingInterval);
        return;
      }

      attempts++;
      if (attempts > maxAttempts) {
        clearInterval(pollingInterval);
        onError(error); // Arreso: passa l'errore al frontend
        return;
      }

      try {
        const response = await fetch(`${API_BASE_URL}/status/${jobId}?key=${API_KEY}`);
        if (!response.ok) throw new Error('Polling fallito');
        
        const data: JobStatusUpdate = await response.json();
        onUpdate(data); // Aggiorna lo stato nell'app esattamente come farebbe la SSE

        // Se il job è terminato, interrompiamo il polling e chiudiamo
        if (data.status === 'completed' || data.status === 'failed') {
          cleanup();
        }
      } catch (pollErr) {
        console.error(`Tentativo di polling ${attempts}/${maxAttempts} fallito:`, pollErr);
      }
    }, 2000); // Intervallo di 2 secondi tra una richiesta e l'altra

    // Notifichiamo temporaneamente che siamo in modalità di recupero (opzionale)
    onUpdate({ status: 'connected', message: 'Connessione persa, recupero in corso (polling)...' });
  };

  return cleanup;
}
export function getDownloadUrl(jobId: string): string {
  return `${API_BASE_URL}/download/${jobId}?key=${API_KEY}`;
}

// 👇 NUOVO: Recupera la lista dei job recenti
export async function fetchRecentJobs(limit: number = 20): Promise<JobStatusUpdate[]> {
  const response = await fetch(`${API_BASE_URL}/jobs?limit=${limit}`, {
    headers: { 'x-api-key': API_KEY }
  });
  if (!response.ok) throw new Error('Impossibile recuperare lo storico');
  return response.json();
}

// 👇 NUOVO: Recupera tutti i dettagli di un singolo job
export async function fetchJobDetails(jobId: string): Promise<JobStatusUpdate> {
  const response = await fetch(`${API_BASE_URL}/status/${jobId}?key=${API_KEY}`);
  if (!response.ok) throw new Error('Impossibile recuperare il job');
  return response.json();
}