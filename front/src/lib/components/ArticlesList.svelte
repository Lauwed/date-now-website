<script lang="ts">
	import Card from './Card.svelte';
	import ArticlePreview from './ArticlePreview.svelte';
	import type { Article } from '../../types';

	interface Props {
		title: string;
		articles: Article[];
	}

	let { title, articles }: Props = $props();
</script>

<Card tag="section" customClass="articles">
	<h2 class="articles__title">{title}</h2>

	<ul class="articles__list">
		{#each articles as article}
			<ArticlePreview {article} />
		{:else}
			<li class="articles__empty">No issue yet. Comme back later 😎</li>
		{/each}
	</ul>
</Card>

<style lang="scss">
	:global(.articles) {
		display: flex;
		flex-direction: column;
	}

	.articles {
		&__list {
			display: grid;
			gap: 12px;
			grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
		}

		&__empty {
			opacity: 0.75;
		}
	}
</style>
