<script lang="ts">
	import ArticlesList from '$lib/components/ArticlesList.svelte';
	import Hero from '$lib/layout/Hero.svelte';
	import type { PageProps } from './$types';

	let { data }: PageProps = $props();
</script>

<Hero />

<ArticlesList title="Latest issues" articles={data.data} />
