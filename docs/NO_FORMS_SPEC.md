# NO-FORMS — Captura de Información No Estructurada
> Feature specification para apps predeterminadas de Spatial OS  
> Versión: 0.1.0 | Estado: PROPUESTA  
> Agentes: `@architect` `@refactor`  
> Dependencias: `spatial-shell-v1`, `spatial-xr-bridge`, `Quick Notes`

---

## Índice

1. [Visión del Feature](#1-visión-del-feature)
2. [El Problema con los Formularios](#2-el-problema-con-los-formularios)
3. [Filosofía de Diseño](#3-filosofía-de-diseño)
4. [Arquitectura del Pipeline](#4-arquitectura-del-pipeline)
5. [Modos de Captura](#5-modos-de-captura)
6. [Motor de Normalización IA](#6-motor-de-normalización-ia)
7. [Modelo de Datos](#7-modelo-de-datos)
8. [Apps Predeterminadas que lo Implementan](#8-apps-predeterminadas-que-lo-implementan)
9. [Visualización en Spatial OS](#9-visualización-en-spatial-os)
10. [Tareas por Agente](#10-tareas-por-agente)
11. [Contratos de Interfaz](#11-contratos-de-interfaz)
12. [Criterios de Aceptación](#12-criterios-de-aceptación)

---

## 1. Visión del Feature

**No-Forms** es el sistema de captura de información de Spatial OS. Elimina los formularios como interfaz de entrada de datos y los reemplaza con captura en lenguaje natural — texto libre, voz, imagen, o conversación — que el sistema normaliza automáticamente usando IA.

El usuario nunca ve un campo de formulario. Ve un espacio donde puede depositar información de cualquier forma, y el sistema se encarga de estructurarla.

```
ANTES (paradigma form):
  Usuario → rellena 12 campos → BD relacional → app muestra datos

DESPUÉS (No-Forms):
  Usuario → habla / escribe / fotografía → IA normaliza → 
  document store → nodos en espacio 3D
```

El resultado no es una fila en una tabla. Es un **nodo vivo** en el espacio tridimensional de Spatial OS, conectado a otros nodos por relaciones que la IA también infiere.

---

## 2. El Problema con los Formularios

Los formularios son una interfaz diseñada en los años 70 para las limitaciones de las bases de datos relacionales, no para los humanos. Sus problemas estructurales:

**Fricción cognitiva forzada.** El usuario debe fragmentar una idea holística en campos discretos. "Conocí a María en la conferencia y quedamos en hablar de su proyecto de AR la próxima semana" se convierte en: nombre, apellido, empresa, email, teléfono, fecha de seguimiento, asunto, notas. El pensamiento es fluido; el form es rígido.

**Estructura predefinida como asunción.** El form asume que quien lo diseñó anticipó todos los campos necesarios. En la práctica, la información real siempre desborda o no encaja con exactitud en el schema definido.

**Repetición punitiva.** El autofill de Apple y los gestores de contraseñas son parches sobre el problema, no soluciones. Siguen requiriendo que el usuario interactúe con la estructura del form.

**Desconexión entre captura y contexto.** Un form de "nuevo contacto" no sabe que esa persona está relacionada con un proyecto activo, una nota de ayer, o un email sin responder. La IA sí puede inferirlo.

---

## 3. Filosofía de Diseño

### El humano habla, la máquina estructura

La responsabilidad de traducir lenguaje natural a estructura de datos es del sistema, no del usuario. El usuario no necesita saber que existe una colección `contacts` con un schema. Solo necesita comunicar información de forma natural.

### Document store sobre relacional

Los datos capturados viven en un **document store** (formato JSON/BSON). No hay schema fijo. Cada documento puede tener campos distintos. La normalización es progresiva — el sistema añade estructura a medida que acumula contexto, no de golpe al momento de captura.

```
// Un contacto capturado por voz puede empezar así:
{
  "_id": "node_a3f2",
  "_type": "contact",
  "_raw": "Conocí a María García de Anthropic España, AR project lead",
  "name": "María García",
  "company": "Anthropic España",
  "role": "AR project lead",
  "_confidence": 0.94,
  "_created": "2026-02-25T17:42:00Z"
}

// Y enriquecerse automáticamente después:
{
  ...
  "email": "maria@anthropic.com",   // añadido al recibir su email
  "project": "node_b1c4",           // inferido por co-aparición
  "follow_up": "2026-03-04",        // extraído de "la próxima semana"
  "_relations": ["node_e7a1", "node_f2b8"]
}
```

### Los nodos reemplazan a los registros

Un contacto no es una fila. Es un nodo que flota en el espacio de Spatial OS, con conexiones visibles a otros nodos — proyectos, notas, eventos. La estructura emerge visualmente, no como tabla.

### Captura sin interrupción de flujo

La captura de información debe poder ocurrir en cualquier momento sin cambiar de contexto. Desde cualquier capa Z del sistema, en cualquier app, con cualquier modo de input.

---

## 4. Arquitectura del Pipeline

```
┌─────────────────────────────────────────────────────────┐
│                   MODOS DE CAPTURA                      │
│   Texto libre │ Voz │ Imagen/OCR │ Conversación IA      │
└───────────────────────┬─────────────────────────────────┘
                        │ raw input
                        ▼
┌─────────────────────────────────────────────────────────┐
│              INGESTION LAYER                            │
│   no-forms-daemon — recibe input de cualquier fuente    │
│   normaliza encoding, añade metadata de contexto        │
│   (app activa, capa Z, timestamp, modo de captura)      │
└───────────────────────┬─────────────────────────────────┘
                        │ RawCapture { text, context }
                        ▼
┌─────────────────────────────────────────────────────────┐
│              NORMALIZACIÓN IA                           │
│   LLM local (llama.cpp / ollama) o API cloud            │
│   → detección de tipo (contacto, tarea, evento, nota)   │
│   → extracción de entidades y relaciones                │
│   → inferencia de campos implícitos                     │
│   → detección de referencias a nodos existentes         │
└───────────────────────┬─────────────────────────────────┘
                        │ NormalizedDocument + Relations[]
                        ▼
┌─────────────────────────────────────────────────────────┐
│              DOCUMENT STORE                             │
│   SurrealDB / CouchDB / archivo JSON local              │
│   Schema-less, queryable, con grafo de relaciones       │
└───────────────────────┬─────────────────────────────────┘
                        │ node_created / node_updated events
                        ▼
┌─────────────────────────────────────────────────────────┐
│              SPATIAL RENDERER                           │
│   Materializa el nodo en el espacio 3D de Spatial OS    │
│   Posición Z según tipo y urgencia                      │
│   Conexiones visibles a nodos relacionados              │
└─────────────────────────────────────────────────────────┘
```

---

## 5. Modos de Captura

### 5.1 Texto libre (Quick Capture)

Atajo global `Super + Space` abre un campo de texto flotante en la capa Z:0 desde cualquier parte del sistema. El usuario escribe en lenguaje natural. No hay labels, no hay campos, no hay placeholders que sugieran estructura.

```
┌────────────────────────────────────────────────┐
│  >_ quedé con Jorge el jueves a las 10 para    │
│     revisar el contrato del proyecto Lisboa     │
│                                          [↵]   │
└────────────────────────────────────────────────┘
```

El sistema infiere: evento (jueves 10:00), participante (Jorge → busca nodo existente), tema (contrato, proyecto Lisboa → busca nodo proyecto).

### 5.2 Voz

Activación por hotword o botón en el HUD de Spatial OS. Transcripción local con Whisper (llama.cpp). El texto transcrito pasa al mismo pipeline que el texto libre. La voz es el modo natural en AR/VR cuando las manos están ocupadas.

```
Usuario (en AR): "oye spatial, añade a la lista de compras 
                  dos paquetes de papel A4 y tinta para la 
                  impresora HP"

Sistema extrae:
  type: shopping_item × 2
  items: [
    { product: "papel A4", quantity: 2, unit: "paquete" },
    { product: "tinta impresora", brand: "HP", quantity: 1 }
  ]
  list: "compras" → nodo existente encontrado
```

### 5.3 Imagen / OCR

El usuario fotografía (o captura pantalla de) un recibo, tarjeta de visita, documento, pizarra con notas. El sistema hace OCR + extracción de entidades.

```
Foto de tarjeta de visita →
  type: contact
  name: "Laura Mendez"
  title: "Senior iOS Engineer"
  company: "Tuenti / Telefónica"
  email: "l.mendez@..."
  phone: "+34 6..."
  _source: "business_card_ocr"
```

```
Foto de recibo de restaurante →
  type: expense
  amount: 47.30
  currency: "EUR"
  vendor: "La Taberna del Puerto"
  date: "2026-02-24"
  category: "meals" // inferido
  _source: "receipt_ocr"
```

### 5.4 Conversación IA (modo chat)

El usuario tiene una conversación libre con el asistente integrado de Spatial OS. A lo largo de la conversación, el sistema extrae información estructurada de forma continua sin interrumpir el flujo.

```
Usuario: "necesito organizar el viaje a Barcelona la semana 
          que viene, vuelo el martes por la mañana"

Nodos creados en background:
  - event: "Viaje Barcelona" | date: próximo martes
  - task: "reservar vuelo" | due: hoy/mañana
  - location: "Barcelona" | linked to event

Usuario: "ah y tengo que reunirme con el equipo de diseño 
          de Zara, son 3 personas"

Nodos actualizados/creados:
  - event existente enriquecido: meeting con "equipo diseño Zara"
  - contacts: 3 contactos pendientes de nombre
  - company: "Zara / Inditex" → busca nodo existente
```

---

## 6. Motor de Normalización IA

### 6.1 Tipos de documento inferidos

| Tipo | Señales en el texto | Campos extraídos |
|------|--------------------|--------------------|
| `contact` | nombre propio, empresa, rol, email, teléfono | name, company, role, email, phone, social |
| `task` | verbos de acción + deadline implícito o explícito | title, due_date, priority, assignee, project |
| `event` | fecha/hora + lugar + participantes | title, datetime, location, attendees, duration |
| `expense` | cantidad + moneda + vendedor | amount, currency, vendor, category, date, project |
| `note` | texto que no encaja en categorías anteriores | title, body, tags, linked_nodes |
| `project` | nombre de proyecto + objetivo + personas | name, description, members, status, deadline |
| `shopping_item` | producto + cantidad + unidad | product, quantity, unit, list, priority |
| `idea` | lenguaje especulativo, "¿y si...?", "podría..." | title, body, linked_project, tags |

### 6.2 Prompt del sistema para normalización

```
Eres el motor de normalización de No-Forms, el sistema de captura 
de Spatial OS. Tu trabajo es extraer estructura de lenguaje natural.

Dado un texto de entrada, devuelve SOLO un objeto JSON con:
- "type": el tipo de documento (contact/task/event/expense/note/
          project/shopping_item/idea)
- "fields": objeto con todos los campos extraídos
- "relations": array de referencias a entidades mencionadas que 
               podrían ser nodos existentes
- "confidence": float 0.0-1.0 de certeza en la clasificación
- "ambiguities": array de campos donde la extracción es incierta
- "raw": el texto original sin modificar

Reglas:
- Infiere fechas relativas ("mañana", "la semana que viene") 
  usando la fecha actual
- Si un campo no está en el texto, no lo inventes — omítelo
- Si hay múltiples tipos posibles, elige el más probable y 
  anota el alternativo en ambiguities
- Las relaciones son strings descriptivos, no IDs
```

### 6.3 Backends de IA soportados

| Backend | Uso | Privacidad | Latencia |
|---------|-----|-----------|---------|
| `ollama` (llama3, mistral) | default, local | total | ~200-800ms |
| `llama.cpp` directo | embedded, sin daemon | total | ~150-600ms |
| Anthropic API (Claude) | nube, mayor precisión | datos salen del equipo | ~300-800ms |
| OpenAI API | nube, alternativa | datos salen del equipo | ~300-700ms |

El backend se configura por el usuario. El default es local (`ollama`) para garantizar privacidad sin conexión.

---

## 7. Modelo de Datos

### 7.1 Documento base (todos los tipos heredan esto)

```typescript
interface SpatialDocument {
  _id:         string          // UUID generado por el sistema
  _type:       DocumentType    // contact | task | event | ...
  _raw:        string          // input original sin procesar
  _source:     CaptureMode     // text | voice | image | conversation
  _created:    ISO8601         // timestamp de creación
  _updated:    ISO8601         // última modificación
  _confidence: number          // 0.0-1.0, certeza del parser IA
  _app:        string          // app desde donde se capturó
  _z_layer:    number          // capa Z donde vive el nodo
  _spatial_pos: Vec3           // posición en el espacio 3D
  _relations:  Relation[]      // conexiones a otros nodos
  _tags:       string[]        // tags libres
  fields:      Record<string, unknown>  // campos extraídos, sin schema fijo
}

interface Relation {
  target_id:   string
  type:        string   // "assigned_to" | "part_of" | "mentioned_in" | ...
  confidence:  number
  inferred:    boolean  // true si la relación la infirió la IA
}
```

### 7.2 Asignación de capa Z por tipo

Los documentos se posicionan automáticamente en el espacio según su tipo y urgencia. El usuario siempre puede moverlos.

```
Z:0  (frente)   → tareas urgentes del día, eventos en próximas 2h
Z:1  (medio)    → tareas esta semana, contactos activos, proyectos en curso
Z:2  (fondo)    → notas de referencia, contactos pasivos, historial
Z:-1 (overlay)  → capturas en proceso de normalización (loading state)
```

### 7.3 Store recomendado

**SurrealDB** — base de datos multi-modelo (document + graph) embebible como librería en C++/Rust. Ideal porque:
- Corre localmente sin servidor
- Soporta queries de grafo nativas (relaciones entre nodos)
- Schema-less por defecto, schema opcional por colección
- Bindings para C++, Rust, y WebAssembly (para la capa WebXR)

```sql
-- Ejemplo de query espacial
SELECT * FROM document 
WHERE _type = 'task' 
AND fields.due_date < time::now() + 7d
FETCH _relations.*;
```

---

## 8. Apps Predeterminadas que lo Implementan

Todas las apps predeterminadas de Spatial OS implementan No-Forms como interfaz de captura primaria. Los formularios tradicionales quedan como interfaz secundaria opcional para usuarios avanzados que necesiten editar campos específicos.

### 8.1 Quick Notes (ya prototipada)
- **Before**: editor de texto con título + cuerpo
- **After**: el cuerpo de la nota pasa por el pipeline No-Forms y el sistema extrae automáticamente tareas mencionadas, personas, fechas y proyectos referenciados — creando nodos enlazados sin que el usuario haga nada adicional

### 8.2 Contactos Espaciales
- **Before**: formulario "Nuevo Contacto" con 10 campos
- **After**: di o escribe quién es la persona. El sistema crea el nodo de contacto y lo conecta a proyectos, conversaciones y notas donde esa persona ya fue mencionada
- Input aceptado: texto libre, voz, foto de tarjeta de visita, email recibido

### 8.3 Calendario Espacial
- **Before**: formulario con título, fecha inicio, fecha fin, invitados, ubicación, notas
- **After**: "reunión con el equipo de diseño el martes a las 3 en la sala B" crea el evento completo, detecta los participantes como nodos de contacto, y verifica conflictos con otros eventos del mismo período
- Los eventos viven como nodos en el espacio — su posición Z refleja su proximidad temporal (hoy = Z:0, esta semana = Z:1, futuro = Z:2)

### 8.4 Gestor de Gastos
- **Before**: formulario con importe, categoría, fecha, proyecto, notas
- **After**: fotografía el recibo o di "gasté 45 euros en material de oficina en El Corte Inglés". El sistema extrae todos los campos, infiere la categoría, y lo enlaza al proyecto activo si existe

### 8.5 Gestor de Proyectos
- **Before**: formularios de creación de proyecto, tarea, subtarea, asignación
- **After**: "proyecto rediseño app, equipo de 4 personas, entrega en abril" crea el nodo proyecto. Cualquier mención futura de ese proyecto en notas, conversaciones o tareas se enlaza automáticamente

### 8.6 Búsqueda Espacial
No es una app de captura sino de recuperación. Acepta queries en lenguaje natural sobre todos los documentos del sistema y devuelve nodos en el espacio 3D ordenados por relevancia (posición Z = relevancia).

```
"¿qué tengo pendiente con María?"
→ muestra: tareas asignadas a María, eventos con María,
           notas que mencionan a María, emails sin responder
→ todos flotando en el espacio, conectados entre sí
```

---

## 9. Visualización en Spatial OS

### 9.1 Estado de un nodo recién capturado

Cuando el usuario captura información, el nodo aparece en la capa Z:-1 (overlay) con un indicador de procesamiento. En menos de un segundo pasa a su capa Z definitiva con una animación de "aterrizaje".

```
[captura] → nodo en Z:-1 pulsando (procesando) →
[normalizado] → nodo vuela a su posición Z final →
[establecido] → nodo estático, conexiones visibles
```

### 9.2 Visualización de relaciones

Las conexiones entre nodos son líneas visibles en el espacio 3D. Su grosor indica la fuerza de la relación. Las relaciones inferidas por IA aparecen en línea punteada hasta que el usuario las confirma.

### 9.3 Ambigüedades y correcciones

Si el sistema no está seguro de algún campo (`_confidence < 0.7`), el nodo muestra un indicador visual de "pendiente de revisión". El usuario puede corregirlo hablando: "no, el evento es el miércoles no el martes" — lo que genera una nueva normalización parcial del mismo documento.

No hay formulario de corrección. La corrección también es lenguaje natural.

---

## 10. Tareas por Agente

### `@architect`

```markdown
@architect TASK-NF-001
Diseñar el protocolo IPC entre no-forms-daemon y las apps cliente.
El daemon corre como servicio del sistema y expone una API para que
cualquier app pueda enviar capturas y recibir documentos normalizados.
Evaluar: D-Bus, Unix socket con JSON, o gRPC.
Entregable: especificación del protocolo con ejemplos de mensajes.

@architect TASK-NF-002
Diseñar el schema flexible de SpatialDocument.
Debe soportar tipos conocidos (contact, task, event...) y tipos
desconocidos definidos por apps de terceros.
Incluir: sistema de validación opcional, migración de documentos,
versionado de schema por tipo.
Entregable: definición de tipos TypeScript/C++ + documentación.

@architect TASK-NF-003
Diseñar el sistema de resolución de referencias entre nodos.
Cuando la IA extrae "María García" en una nueva captura, el sistema
debe decidir si crear un nodo nuevo o enlazar a uno existente.
Definir: algoritmo de matching por nombre + contexto, umbral de
confianza, resolución de conflictos.
Entregable: diagrama de flujo del algoritmo + API de EntityResolver.

@architect TASK-NF-004
Diseñar la integración de No-Forms con el sistema de capas Z.
Definir cómo se asigna posición Z inicial a un nodo, cómo cambia
con el tiempo (una tarea urgente baja a Z:0 automáticamente),
y cómo el usuario puede overridear la posición.
Entregable: documento de reglas de posicionamiento automático.
```

### `@refactor`

```markdown
@refactor TASK-NF-101
Implementar no-forms-daemon en Rust.
Servicio del sistema que: recibe capturas por IPC, llama al backend
de IA (ollama por defecto), parsea el JSON de respuesta, guarda en
SurrealDB, emite eventos a los subscribers (apps del sistema).
Incluir tests de integración con mocks del backend IA.

@refactor TASK-NF-102
Implementar el cliente No-Forms para Quick Notes (C++ / webview).
Interceptar el texto del editor al guardar, enviarlo al daemon,
y materializar los nodos inferidos (tareas, contactos, fechas)
como nodos flotantes enlazados a la nota en el espacio 3D.

@refactor TASK-NF-103
Implementar el modo de captura por voz.
Integrar whisper.cpp para transcripción local.
Activación: hotword "oye spatial" o atajo Super+V.
Output: texto transcrito → pipeline estándar de No-Forms.
Requisito: funcionar offline, latencia < 1s en hardware mid-range.

@refactor TASK-NF-104
Implementar el OCR para captura por imagen.
Usar tesseract-ocr + post-procesamiento con el LLM local para
interpretar el contenido (tarjeta de visita vs recibo vs documento).
Input: imagen desde cámara (AR), captura de pantalla, o archivo.

@refactor TASK-NF-105
Implementar EntityResolver — deduplicación de nodos.
Dado un nuevo documento con entidades extraídas, buscar en SurrealDB
si ya existen nodos compatibles. Umbral configurable de similaridad.
Algoritmo base: embedding de texto + cosine similarity, fallback a
Levenshtein para nombres propios.

@refactor TASK-NF-106
Implementar la UI de "nodo en procesamiento" en Spatial Shell.
Cuando se captura algo, mostrar el nodo en estado loading en Z:-1
con animación de pulso. Al completar, animar el vuelo hasta la
posición Z final. Si hay ambigüedades, mostrar indicador visual.
```

---

## 11. Contratos de Interfaz

### 11.1 API del daemon (IPC)

```typescript
// Cliente → Daemon
interface CaptureRequest {
  id:       string        // UUID del request
  input:    string        // texto transcrito / extraído
  mode:     'text' | 'voice' | 'image' | 'conversation'
  context: {
    app:      string      // app que origina la captura
    z_layer:  number      // capa activa al momento de captura
    timestamp: ISO8601
    hint?:    DocumentType  // sugerencia de tipo si la app lo sabe
  }
}

// Daemon → Cliente
interface NormalizeResponse {
  request_id:  string
  document:    SpatialDocument
  new_nodes:   SpatialDocument[]   // nodos creados por relaciones
  linked_nodes: string[]           // IDs de nodos existentes enlazados
  ambiguities: Ambiguity[]
  processing_ms: number
}

interface Ambiguity {
  field:       string
  value:       unknown
  confidence:  number
  alternatives: unknown[]
}
```

### 11.2 Eventos del sistema (pub/sub)

```typescript
// El daemon emite estos eventos al spatial-shell y a las apps
type NoFormsEvent =
  | { type: 'node.created';  document: SpatialDocument }
  | { type: 'node.updated';  id: string; diff: Partial<SpatialDocument> }
  | { type: 'node.linked';   source_id: string; target_id: string; relation: string }
  | { type: 'capture.processing'; request_id: string }
  | { type: 'capture.complete';   request_id: string; document_id: string }
  | { type: 'capture.ambiguous';  request_id: string; ambiguities: Ambiguity[] }
```

### 11.3 Interface del backend IA

```rust
// Trait que todos los backends deben implementar
trait AIBackend: Send + Sync {
    async fn normalize(&self, input: &str, context: &CaptureContext) 
        -> Result<NormalizedDocument, AIError>;
    
    async fn is_available(&self) -> bool;
    fn backend_name(&self) -> &str;
    fn supports_offline(&self) -> bool;
}

// Implementaciones
struct OllamaBackend   { model: String, endpoint: Url }
struct LlamaCppBackend { model_path: PathBuf, n_threads: usize }
struct AnthropicBackend { api_key: String, model: String }
```

---

## 12. Criterios de Aceptación

### Precisión de normalización
- [ ] Extracción correcta de tipo de documento ≥ 90% en corpus de prueba
- [ ] Extracción de campos principales (nombre, fecha, cantidad) ≥ 85%
- [ ] Detección de referencias a nodos existentes ≥ 80%
- [ ] Tasa de falsos positivos en deduplicación < 5%

### Rendimiento
- [ ] Captura de texto → nodo visible en espacio 3D < 1.5s (backend local)
- [ ] Transcripción de voz (10 segundos de audio) < 1s en CPU mid-range
- [ ] OCR de imagen estándar < 2s
- [ ] Sin degradación de rendimiento del compositor durante normalización

### Experiencia de usuario
- [ ] El usuario nunca ve un formulario tradicional en el flujo primario
- [ ] Las ambigüedades se resuelven con lenguaje natural, no con campos
- [ ] Posición Z de nodos coincide con intuición del usuario en > 80% de casos
- [ ] Modo offline 100% funcional con backend local

### Privacidad
- [ ] Backend local (ollama) es el default instalado
- [ ] Ningún dato sale del equipo sin consentimiento explícito del usuario
- [ ] Los datos del document store están cifrados en reposo (SQLCipher o equivalente)
- [ ] El usuario puede exportar todos sus datos en JSON estándar en cualquier momento

---

## Referencias y Proyectos Relacionados

- **Mem.ai** — captura no estructurada + IA, referencia de UX
- **Notion AI** — normalización parcial de texto libre
- **SurrealDB** — `surrealdb.com`, store recomendado
- **Ollama** — `ollama.ai`, runtime LLM local
- **whisper.cpp** — `github.com/ggerganov/whisper.cpp`
- **Tesseract OCR** — `github.com/tesseract-ocr/tesseract`
- **llama.cpp** — `github.com/ggerganov/llama.cpp`
- **Apple Intelligence** — referencia de autofill inteligente (paradigma a superar)

---

*Feature spec — Spatial OS / No-Forms*  
*Conectado con: `SPATIAL_OS_SPEC.md`, `Quick Notes`*  
*Última actualización: Febrero 2026*
