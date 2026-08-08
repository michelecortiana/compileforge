import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.tsx'

// 👇 AGGIUNGI QUESTI DUE IMPORT
import * as monaco from 'monaco-editor';
import { loader } from '@monaco-editor/react';

// 👇 AGGIUNGI QUESTA RIGA: Costringe il wrapper a usare il pacchetto locale
loader.config({ monaco });

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)