export interface Color {
	r: number;
	g: number;
	b: number;
}

export interface Tag {
	name: string;
	/** Hexadécimal, tel que renvoyé par l'API — ex. `#dbc1c1`. */
	color: string;
}

export interface Block {
	tag: keyof svelteHTML.IntrinsicElements;
	content?: string;
	children?: Block[];
	class?: string;
	attributes?: { [key: string]: string };
}

export interface Response<T> {
	message?: string;
	success: boolean;
	data: T | null;
	status: number;
}

export interface ResponseMany<T> {
	data: T[];
	total: number;
	totalPages: number;
	count: number;
}

export interface IssueArticle {
	id: number;
	sectionId: number;
	position: number;
	title: string;
	sourceName: string;
	sourceUrl: string;
	/** Markdown, rendered to HTML server-side. */
	summary: string;
}

export interface IssueSection {
	id: number;
	issueId: number;
	position: number;
	type: 'CATEGORY' | 'TEXT';
	categoryName: string | null;
	/** Markdown, rendered to HTML server-side. Null on CATEGORY sections. */
	textBody: string | null;
	articles: IssueArticle[];
}

export interface Media {
	id: number;
	alt: string | null;
	url: string | null;
	thumbUrl: string | null;
	width: number | null;
	height: number | null;
}

/**
 * Auteur tel qu'affiché publiquement. L'API renvoie davantage de champs, dont
 * l'email : ne jamais les remonter jusqu'au template.
 */
export interface Author {
	id: number;
	username: string | null;
	link: string | null;
	picture: Media | null;
}

export interface Article {
	id: number;
	slug: string;
	cover: Media | null;
	authors: Author[];
	title: string;
	subtitle?: string;
	publishedAt: number;
	issueNumber: number;
	excerpt: string;
	status: 'DRAFT' | 'PUBLISHED' | 'ARCHIVE';
	sections: IssueSection[];
	tags: Tag[];
}
