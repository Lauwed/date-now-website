<script lang="ts">
	import { format } from 'date-fns';
	import type { Article } from '../../types';
	import Card from './Card.svelte';
	import Tag from './Tag.svelte';
	import Button from './Button.svelte';

	type ArticleSize = 'medium' | 'large';

	interface Props {
		article: Article;
		size?: ArticleSize;
	}

	let { article, size = 'medium' }: Props = $props();
</script>

<Card
	tag="li"
	customClass={`article-preview ${size !== 'medium' ? `article-preview--${size}` : ''}`}
	--background-opacity="0.15"
	--blur="0px"
>
	<div class="article-preview__header">
		<h3 class="article-preview__title">{article.title}</h3>
		{#if article.subtitle}
			<p class="article-preview__subtitle">{article.subtitle}</p>
		{/if}

		<p class="article-preview__metadata">
			<Tag tag="span">Issue #{article.editionNumber}</Tag>
			<span>{format(article.publishedDate, 'dd/MM/yyyy')}</span>
		</p>

		<ul class="article-preview__tags">
			{#each article.tags as tag}
				<Tag tag="li" --color={`${tag.color.r}, ${tag.color.g}, ${tag.color.b}`}>{tag.name}</Tag>
			{/each}
		</ul>
	</div>

	<img
		class="article-preview__cover"
		src={article.coverURL || '/article-cover-placeholder.png'}
		alt="Issue's cover"
	/>

	<div class="article-preview__content">
		<!-- EXCERPT -->
		<p class="article-preview__excerpt">{article.excerpt}</p>

		<Button --width="100%" customClass="article-preview__button" tag="a" href="/issue"
			>Read more</Button
		>
	</div>
</Card>

<style lang="scss">
	@use './../styles/mixins' as *;

	:global(.article-preview) {
		display: grid;
		gap: 8px;
		grid-row: span 3;
		grid-template-rows: subgrid;

		p {
			margin: 0;
		}
	}

	:global(.article-preview--large) {
		grid-template-columns: 35% 1fr;
		column-gap: 24px;

		.article-preview {
			&__header {
				grid-area: 1 / 2 / 2 / 3;
			}

			&__cover {
				grid-area: 1 / 1 / 3 / 2;
				margin-bottom: 0;
			}

			&__content {
				grid-area: 2 / 2 / 3 / 3;
			}
		}
	}

	:global(.article-preview__button) {
		order: 4;
	}

	.article-preview {
		&__header {
			grid-row: 2 / 3;
			grid-column: span 2;
			display: flex;
			flex-direction: column;
			gap: 4px;
		}
		&__title {
			order: 1;
			font-size: 1.875rem;
			line-height: 1.1;
			margin: 4px 0;

			@include md {
				font-size: 2rem;
			}
		}

		&__subtitle {
			order: 1;
			opacity: 0.75;
		}

		&__metadata {
			display: flex;
			align-items: center;
			gap: 8px;
			order: 0;
		}

		&__cover {
			grid-row: 1 / 2;
			grid-column: span 2;
			display: block;
			border-radius: 6px;
			margin-bottom: 10px;
			object-fit: cover;
			object-position: center;
		}

		&__tags {
			order: 2;
			display: flex;
			gap: 8px;
			flex-wrap: wrap;
		}

		&__content {
			grid-row: 3 / 4;
			grid-column: span 2;
			display: flex;
			flex-direction: column;
			gap: 12px;
		}

		&__excerpt {
			flex: 1;
		}
	}
</style>
