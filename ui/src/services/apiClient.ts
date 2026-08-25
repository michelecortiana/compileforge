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
    console.warn("Connessione SSE caduta. Avvio fallback di polling...", error);
    
    if (eventSource) {
      eventSource.close();
    }

    if (isClosedManually) return;
    let attempts = 0;
    const maxAttempts = 20; 

    pollingInterval = setInterval(async () => {
      if (isClosedManually) {
        clearInterval(pollingInterval);
        return;
      }

      attempts++;
      if (attempts > maxAttempts) {
        clearInterval(pollingInterval);
        onError(error);
        return;
      }

      try {
        const response = await fetch(`${API_BASE_URL}/status/${jobId}?key=${API_KEY}`);
        if (!response.ok) throw new Error('Polling fallito');
        
        const data: JobStatusUpdate = await response.json();
        onUpdate(data); 
        if (data.status === 'completed' || data.status === 'failed') {
          cleanup();
        }
      } catch (pollErr) {
        console.error(`Tentativo di polling ${attempts}/${maxAttempts} fallito:`, pollErr);
      }
    }, 2000); 
    onUpdate({ status: 'connected', message: 'Connessione persa, recupero in corso (polling)...' });
  };

  return cleanup;
}
export function getDownloadUrl(jobId: string): string {
  return `${API_BASE_URL}/download/${jobId}?key=${API_KEY}`;
}

export async function fetchRecentJobs(limit: number = 20): Promise<JobStatusUpdate[]> {
  const response = await fetch(`${API_BASE_URL}/jobs?limit=${limit}`, {
    headers: { 'x-api-key': API_KEY }
  });
  if (!response.ok) throw new Error('Impossibile recuperare lo storico');
  return response.json();
}

export async function fetchJobDetails(jobId: string): Promise<JobStatusUpdate> {
  const response = await fetch(`${API_BASE_URL}/status/${jobId}?key=${API_KEY}`);
  if (!response.ok) throw new Error('Impossibile recuperare il job');
  return response.json();
}