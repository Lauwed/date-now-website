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
	content?: String;
	children?: Block[];
	class?: string;
	attributes?: { [key: string]: string };
}

export interface Article {
	coverURL?: string;
	title: string;
	subtitle?: string;
	publishedDate: Date;
	editionNumber: number;
	excerpt: string;
	content: Block[];
	tags: Tag[];
}
