import { PUBLIC_API_URL } from '$env/static/public';
import { marked } from 'marked';
import type { Article, Author, IssueSection, Response } from '../../../types';
import type { PageServerLoad } from './$types';

export type RenderedSection = Omit<IssueSection, 'textBody' | 'articles'> & {
	/** Markdown already rendered to HTML — never render it client-side. */
	textBodyHtml: string | null;
	articles: (Omit<IssueSection['articles'][number], 'summary'> & { summaryHtml: string })[];
};

export type IssuePageData = Response<Article> & {
	isPreview: boolean;
	sections: RenderedSection[];
};

/**
 * Markdown is authored by the team through the CMS, never by visitors, so it
 * is rendered on the server and injected with {@html} without sanitising.
 * That assumption has to be revisited the day untrusted authors can write it.
 */
function render(markdown: string | null): string | null {
	if (!markdown) return null;
	return marked.parse(markdown, { async: false });
}

/**
 * L'API renvoie l'objet utilisateur complet — email, rôle, dates. SvelteKit
 * sérialise dans la page tout ce que `load` retourne, donc ces champs
 * partiraient chez chaque visiteur même sans être affichés : on ne garde que
 * ce qui est public.
 */
function publicAuthors(authors: Author[] = []): Author[] {
	return authors.map(({ id, username, link, picture }) => ({ id, username, link, picture }));
}

function renderSections(sections: IssueSection[] = []): RenderedSection[] {
	return [...sections]
		.sort((a, b) => a.position - b.position)
		.map(({ textBody, articles, ...section }) => ({
			...section,
			textBodyHtml: render(textBody),
			articles: [...(articles ?? [])]
				.sort((a, b) => a.position - b.position)
				.map(({ summary, ...article }) => ({
					...article,
					summaryHtml: render(summary) ?? ''
				}))
		}));
}

export const load: PageServerLoad = async ({ params, url, setHeaders }): Promise<IssuePageData> => {
	const preview = url.searchParams.get('preview');

	// A preview URL carries a bearer-like token: keep it out of caches, out of
	// search engines, and out of the Referer header sent to third parties.
	if (preview) {
		setHeaders({
			'cache-control': 'no-store',
			'x-robots-tag': 'noindex, nofollow',
			'referrer-policy': 'no-referrer'
		});
	}

	const endpoint = new URL(`${PUBLIC_API_URL}/api/issue/slug/${params.slug}`);
	if (preview) endpoint.searchParams.set('preview', preview);

	const res = await fetch(endpoint, { method: 'GET' });

	if (!res.ok) {
		return {
			success: false,
			data: null,
			message: res.status === 404 ? 'This issue does not exist or is not published yet.' : 'Something went wrong',
			status: res.status,
			isPreview: Boolean(preview),
			sections: []
		};
	}

	const data: Article = await res.json();

	return {
		success: true,
		data: { ...data, authors: publicAuthors(data.authors) },
		status: res.status,
		isPreview: Boolean(preview),
		sections: renderSections(data.sections)
	};
};
