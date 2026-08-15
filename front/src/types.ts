export interface Color {
	r: number;
	g: number;
	b: number;
}

export interface Tag {
	name: string;
	color: Color;
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

export interface Article {
	id: number;
	slug: string;
	coverURL?: string;
	title: string;
	subtitle?: string;
	publishedAt: number;
	issueNumber: number;
	excerpt: string;
	content: Block[];
	tags: Tag[];
}
