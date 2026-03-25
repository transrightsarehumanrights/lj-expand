import React from 'react';
import ReactMarkdown from 'react-markdown';
import styles from './NamespaceDocs.module.css';

function Markdown({ children }) {
  if (!children) return null;
  return <ReactMarkdown>{children}</ReactMarkdown>;
}

function buildSignature(namespace, func) {
  const base = namespace === 'base' ? 'lje' : `lje.${namespace}`;
  const params = func.params
    .map(p => p.optional ? `${p.name}?` : p.name)
    .join(', ');
  return `${base}.${func.name}(${params})`;
}

function NoteBanner({ note }) {
  if (!note) return null;
  return (
    <div className={styles.note}>
      <span><strong>Note:</strong> {note}</span>
    </div>
  );
}

function DeprecatedBanner({ deprecated }) {
  if (!deprecated) return null;
  const message = typeof deprecated === 'string' ? deprecated : 'This function is deprecated and may be removed in a future version.';
  return (
    <div className={styles.deprecated}>
      <span><strong>Deprecated:</strong> {message}</span>
    </div>
  );
}

function FunctionDoc({ func, namespace }) {
  return (
    <div className={styles.functionDoc}>
      <h3 id={func.name}>
        <code>{buildSignature(namespace, func)}</code>
      </h3>

      <DeprecatedBanner deprecated={func.deprecated} />
      <NoteBanner note={func.note} />
      <Markdown>{func.description}</Markdown>

      {func.params.length > 0 && (
        <>
          <h4>Parameters</h4>
          <table>
            <thead>
              <tr>
                <th>Name</th>
                <th>Type</th>
                <th>Description</th>
              </tr>
            </thead>
            <tbody>
              {func.params.map(p => (
                <tr key={p.name}>
                  <td><code>{p.name}{p.optional ? '?' : ''}</code></td>
                  <td><code>{p.type}</code></td>
                  <td><Markdown>{p.description}</Markdown></td>
                </tr>
              ))}
            </tbody>
          </table>
        </>
      )}

      {func.returns?.length > 0 && (
        <>
          <h4>Returns</h4>
          <table>
            <thead>
              <tr>
                <th>Type</th>
                <th>Description</th>
              </tr>
            </thead>
            <tbody>
              {func.returns.map((r, i) => (
                <tr key={i}>
                  <td><code>{r.type}{r.optional ? '?' : ''}</code></td>
                  <td><Markdown>{r.description}</Markdown></td>
                </tr>
              ))}
            </tbody>
          </table>
        </>
      )}

      {func.errors?.length > 0 && (
        <>
          <h4>Errors</h4>
          <ul>
            {func.errors.map((e, i) => (
              <li key={i}><code>{e}</code></li>
            ))}
          </ul>
        </>
      )}
    </div>
  );
}

function ConstantsSection({ constants }) {
  if (!constants?.length) return null;
  return (
    <>
      <h2 id="constants">Constants</h2>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          {constants.map(c => (
            <tr key={c.name}>
              <td><code>{c.name}</code></td>
              <td><code>{c.type}</code></td>
              <td><Markdown>{c.description}</Markdown></td>
            </tr>
          ))}
        </tbody>
      </table>
    </>
  );
}

export default function NamespaceDocs({ data }) {
  return (
    <div>
      <p className={styles.namespaceDescription}>{data.description}</p>
      <ConstantsSection constants={data.constants} />
      {data.functions?.length > 0 && <h2 id="functions">Functions</h2>}
      {data.functions?.map(func => (
        <FunctionDoc key={func.name} func={func} namespace={data.namespace} />
      ))}
    </div>
  );
}
