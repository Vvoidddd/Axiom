(() => {
  const button = document.querySelector('[data-copy]');
  if (!button || !navigator.clipboard) return;

  button.addEventListener('click', async () => {
    const original = button.textContent;
    try {
      await navigator.clipboard.writeText(button.dataset.copy);
      button.textContent = 'Copied';
    } catch {
      button.textContent = 'Select text';
    }
    window.setTimeout(() => { button.textContent = original; }, 1600);
  });
})();
