import type { CompileRequest, CompileResponse, ErrorResponse } from '../types/api';

// Ri-esportiamo se servono altrove, oppure usali direttamente
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
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'connected';
  message?: string;
  output?: string;
  errorMessage?: string;
  assembly?: string;
  irCode?: string;
  createdAt?: string;
  finishedAt?: string;
  downloadUrl?: string; 
}

/**
 * Apre una connessione SSE per ascoltare gli aggiornamenti di un job specifico.
 */
export function listenForJobStatus(
  jobId: string,
  onUpdate: (data: JobStatusUpdate) => void,
  onError: (error: Event) => void
): () => void {
  // 👇 RIPRISTINATO: Il query parameter ?key= è necessario per EventSource
  const streamUrl = `${API_BASE_URL}/status/${jobId}/stream?key=${API_KEY}`;
  
  const eventSource = new EventSource(streamUrl);

  eventSource.onmessage = (event) => {
    try {
      const data: JobStatusUpdate = JSON.parse(event.data);
      onUpdate(data);

      if (data.status === 'completed' || data.status === 'failed') {
        eventSource.close();
      }
    } catch (err) {
      console.error("Errore nel parsing del messaggio SSE:", err);
    }
  };

  eventSource.onerror = (error) => {
    console.error("Errore nella connessione SSE:", error);
    eventSource.close();
    onError(error);
  };

  return () => {
    if (eventSource.readyState !== EventSource.CLOSED) {
      eventSource.close();
    }
  };
}

export function getDownloadUrl(jobId: string): string {
  // 👇 RIPRISTINATO: Il query parameter ?key= è necessario per i link di download diretto
  return `${API_BASE_URL}/download/${jobId}?key=${API_KEY}`;
}