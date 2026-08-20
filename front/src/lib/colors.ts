/**
 * Le mixin `glass` attend un triplet « r, g, b » ; l'API renvoie une couleur
 * hexadécimale. Retombe sur du blanc si la valeur est inexploitable.
 */
export function rgbOf(hex: string): string {
	const value = (hex ?? '').replace('#', '').trim();
	const full =
		value.length === 3
			? value
					.split('')
					.map((c) => c + c)
					.join('')
			: value;

	if (full.length !== 6) return '255, 255, 255';

	const int = parseInt(full, 16);
	if (Number.isNaN(int)) return '255, 255, 255';

	return `${(int >> 16) & 255}, ${(int >> 8) & 255}, ${int & 255}`;
}
